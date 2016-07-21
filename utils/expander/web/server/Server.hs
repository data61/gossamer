{-# LANGUAGE OverloadedStrings, ScopedTypeVariables #-}

{- 
Depends on:
    - happstack-lite-7.1.1
    - direct-sqlite-1.1

but will probably work with other versions of these libraries.
-}

module Main where

import System.IO
import System.Environment
import System.Random
import System.CPUTime
import System.Console.GetOpt
import Control.Monad.Base
import Control.Applicative ((<$>), optional)
import Data.Maybe (fromMaybe)
import Data.Char (isDigit)
import Data.List (intersperse, partition)
import Data.Text (Text)
import Data.Text.Lazy (unpack)
import Happstack.Lite
import Text.Blaze.Html5 (Html, (!), a, form, input, p, toHtml, label)
import Text.Blaze.Html5.Attributes (action, enctype, href, name, size, type_, value)
import qualified Text.Blaze.Html5 as H
import qualified Text.Blaze.Html5.Attributes as A

import qualified Database.SQLite3 as SQL

--------------------------------------------------------------------------------

data Flag = 
      ContigDb String
    | HtmlBase String
    | Port String
    | Help
  deriving Show

type Flags = [Flag]

options :: [OptDescr Flag]
options = 
    [ Option ['c']  ["contig-db"]       (ReqArg ContigDb "FILE")    "Contig database file"
    , Option ['b']  ["html-base"]       (ReqArg HtmlBase "DIR")     "Base directory for HTML files"
    , Option ['p']  ["port"]            (ReqArg Port "PORT")        "Port to serve on (default 8000)"
    , Option ['h']  ["help"]            (NoArg Help)                "Print help and exit"
    ]

printUsage :: IO ()
printUsage = do
    p <- getProgName
    hPutStrLn stderr $ usageInfo (header p) options
  where
    header p = "usage: " ++ p ++ " -b <DIR> -c <FILE> [OPTION...]"

contigDb :: Flags -> String
contigDb fs = case [ db | ContigDb db <- fs ] of 
    [] -> ""
    db:_ -> db

htmlBase :: Flags -> String
htmlBase fs = case [ dir | HtmlBase dir <- fs ] of
    [] -> ""
    dir:_ -> dir

portInt :: Flags -> Int
portInt fs = case [ read n | Port n <- fs, all isDigit n ] of
    [] -> 8000
    n:_ -> n

help :: Flags -> Bool
help fs = not $ null [ h | h@Help <- fs ]

--------------------------------------------------------------------------------

main :: IO ()
main = do
    as <- getArgs
    case getOpt Permute options as of
        (_, _, errs@(_:_)) -> hPutStrLn stderr (concat errs) >> printUsage
        (_, _:_, _) -> printUsage
        (fs, _, []) 
            | help fs -> printUsage
            | null $ contigDb fs -> hPutStrLn stderr "error: no contig database!" >> printUsage
            | null $ htmlBase fs -> hPutStrLn stderr "error: no HTML base directory!" >> printUsage
            | otherwise -> serve (Just config) (server fs)
          where
            config = defaultServerConfig { port = portInt fs }

server :: Flags -> ServerPart Response
server fs = msum
    [ dir "node"    $ node fs
    , dir "gene"    $ gene fs
    , dir "links_fwd" $ linksFwd fs
    , dir "links_bck" $ linksBck fs
    , dir "align_fwd" $ alignFwd fs
    , dir "align_bck" $ alignBck fs
    , dir "alignment" $ alignment fs
    , dir "sequence" $ Main.sequence fs
    , dir "expander" $ expander fs
    , dir "lib" $ libDir fs
    , dir "css" $ cssDir fs
    , homePage
    ]

expander :: Flags -> ServerPart Response
expander fs = serveFile (asContentType "text/html") (htmlBase fs ++ "/expander.html")

libDir :: Flags -> ServerPart Response
libDir fs = serveDirectory DisableBrowsing [] (htmlBase fs ++ "/lib")

cssDir :: Flags -> ServerPart Response
cssDir fs = serveDirectory DisableBrowsing [] (htmlBase fs ++ "/css")


template :: Text -> Html -> Response
template title body = toResponse $
    H.html $ do
        H.head $ do
            H.title (toHtml title)
        H.body $ do
            body
            p $ a ! href "/" $ "back home"

homePage :: ServerPart Response
homePage = 
    ok $ template "home page" $ do
            H.h1 "Hello!"
            H.p "Check out the following."
            H.p $ a ! href "/node/1234" $ "node"
            H.p $ a ! href "/sequence/1234" $ "sequence"
            H.p $ a ! href "/expander" $ "expander"

node :: Flags -> ServerPart Response
node fs = 
    path $ \(id :: String) -> do
        ret <- liftBase $ do
            db <- SQL.open (contigDb fs)
            stmt <- SQL.prepare db $ "SELECT * FROM nodes WHERE id = " ++ id ++ ";"
            res <- SQL.step stmt
            case res of
                SQL.Done -> return dummy
                SQL.Row -> do
                    SQL.SQLInteger id <- SQL.column stmt 0
                    SQL.SQLInteger rc <- SQL.column stmt 1
                    SQL.SQLFloat cov_mean <- SQL.column stmt 2
                    SQL.SQLInteger length <- SQL.column stmt 3
                    SQL.finalize stmt
                    SQL.close db
                    return $ json id rc cov_mean length 
        ok $ toResponse ret
  where
    -- json :: Int64 -> Int64 -> Double -> Int64 -> String
    json id rc c l = "{ \"id\": " ++ show id ++ 
                     ", \"rc\": " ++ show rc ++ 
                     ", \"cov_mean\": " ++ show c ++
                     ", \"length\": " ++ show l ++ " }"
    dummy = json id id 0 0

gene :: Flags -> ServerPart Response 
gene fs = 
    path $ \(id :: String) -> do
        ret <- liftBase $ do
            db <- SQL.open (contigDb fs)
            stmt <- SQL.prepare db $ "SELECT id FROM alignments WHERE gene = '" ++ id ++ "';"
            itms <- loop stmt []
            let txt = "[" ++ concat (intersperse "," itms) ++ "]"
            -- putStrLn (show $ length itms)
            SQL.finalize stmt
            SQL.close db
            return txt
        ok $ toResponse ret
  where
    loop :: SQL.Statement -> [String] -> IO [String]
    loop stmt acc = do
        res <- SQL.step stmt
        case res of 
            SQL.Done -> return acc
            SQL.Row -> do
                SQL.SQLInteger id <- SQL.column stmt 0
                loop stmt (("{\"id\": " ++ show id ++ "}") : acc) 

links :: Flags -> String -> ServerPart Response
links fs query = do
    ret <- liftBase $ do
        db <- SQL.open (contigDb fs)
        stmt <- SQL.prepare db query
        itms <- loop stmt []
        SQL.finalize stmt
        SQL.close db
        return $ "[" ++ concat (intersperse "," itms) ++ "]"
    ok $ toResponse ret
  where
    loop :: SQL.Statement -> [String] -> IO [String]
    loop stmt acc = do
        res <- SQL.step stmt
        case res of 
            SQL.Done -> return acc
            SQL.Row -> do
                SQL.SQLInteger id_from <- SQL.column stmt 0
                SQL.SQLInteger id_to <- SQL.column stmt 1
                SQL.SQLInteger gap <- SQL.column stmt 2
                SQL.SQLInteger count <- SQL.column stmt 3
                SQL.SQLInteger typ <- SQL.column stmt 4
                loop stmt (json id_from id_to gap count typ : acc)
      where
        json a b g c t = "{ \"id_from\": " ++ show a ++ 
                         ", \"id_to\": " ++ show b ++ 
                         ", \"gap\": " ++ show g ++
                         ", \"count\": " ++ show c ++
                         ", \"type\": " ++ typeStr t ++ " }"
        typeStr 1 = "\"PAIR\""
        typeStr _ = "\"\""

linksFwd :: Flags -> ServerPart Response
linksFwd fs = 
    path $ \(id :: String) -> do
        let query = "SELECT * FROM links WHERE id_from = " ++ id ++ ";"
        links fs query

linksBck :: Flags -> ServerPart Response
linksBck fs = 
    path $ \(id :: String) -> do
        let query = "SELECT * FROM links WHERE id_to = " ++ id ++ ";"
        links fs query


type AlignQueryFn = (String, Int, String) -> String

-- String format:
--  start +/- name
aligns :: Flags -> AlignQueryFn -> ServerPart Response
aligns fs f = 
    path $ \(q :: String) -> do
        ret <- liftBase $ do
            db <- SQL.open (contigDb fs)
            let query = f $ parse q
            stmt <- SQL.prepare db query
            res <- SQL.step stmt
            case res of 
                SQL.Done -> return "{}"
                SQL.Row -> do
                    SQL.SQLInteger id <- SQL.column stmt 0
                    return $ "{\"id\": " ++ show id ++ " }" 
        ok $ toResponse ret
  where
    parse q = case q' of
                ('-':ref) -> (start, -1, ref)
                ('+':ref) -> (start, 1, ref)
                _ -> ("0", 1, "")
      where
        (start, q') = span isDigit q

alignFwd :: Flags -> ServerPart Response
alignFwd fs = aligns fs f
  where
    f (pos, dir, ref) = "SELECT id FROM alignments WHERE start > " ++ pos ++ " AND dir == " ++ show dir ++ " AND name == '" ++ ref ++ "' ORDER BY start ASC LIMIT 1;"

alignBck :: Flags -> ServerPart Response
alignBck fs = aligns fs f
  where
    f (pos, dir, ref) = "SELECT id FROM alignments WHERE end < " ++ pos ++ " AND dir == " ++ show dir ++ " AND name == '" ++ ref ++ "' ORDER BY end DESC LIMIT 1;"

sequence :: Flags -> ServerPart Response
sequence fs = 
    path $ \(id :: String) -> do
        ret <- liftBase $ do
            db <- SQL.open (contigDb fs) 
            stmt <- SQL.prepare db $ "SELECT * FROM sequences WHERE id = " ++ id ++ ";"
            SQL.step stmt
            seq <- SQL.column stmt 1
            SQL.finalize stmt
            SQL.close db
            case seq of 
                SQL.SQLText s -> return $ "\"" ++ s ++ "\""
                _ -> return "\"\""
        ok $ toResponse ret

alignment :: Flags -> ServerPart Response
alignment fs = 
    path $ \(id :: String) -> do
        ret <- liftBase $ do
            db <- SQL.open (contigDb fs)
            stmt <- SQL.prepare db $ "SELECT * FROM alignments WHERE id = " ++ id ++ ";"
            res <- SQL.step stmt
            val <- case res of
                SQL.Done -> return dummy
                SQL.Row -> do
                    SQL.SQLInteger id <- SQL.column stmt 0
                    SQL.SQLText ref <- SQL.column stmt 1
                    SQL.SQLInteger start <- SQL.column stmt 2
                    SQL.SQLInteger end <- SQL.column stmt 3
                    SQL.SQLInteger matched <- SQL.column stmt 4
                    SQL.SQLInteger dir <- SQL.column stmt 5
                    SQL.SQLText gene <- SQL.column stmt 6
                    return $ json id ref start end matched dir gene
            SQL.finalize stmt
            SQL.close db
            return val
        ok $ toResponse ret
  where
    json a r s e m d g = "{ \"id\": " ++ show a ++
                         ", \"name\": " ++ show r ++
                         ", \"start\": " ++ show s ++
                         ", \"end\": " ++ show e ++
                         ", \"matched\": " ++ show m ++
                         ", \"dir\": " ++ show (dirStr d) ++
                         ", \"gene\": " ++ show g ++ " }"
    dirStr x = if x >= 0 then "+" else "-"
    dummy = json 0 "" 0 0 0 0 ""
