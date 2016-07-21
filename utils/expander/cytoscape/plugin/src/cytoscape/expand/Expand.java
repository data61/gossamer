
package cytoscape.expand;

import cytoscape.Cytoscape;
import cytoscape.CyEdge;
import cytoscape.CyNetwork;
import cytoscape.CyNode;
import cytoscape.data.CyAttributes;
import cytoscape.data.Semantics;

import cytoscape.layout.AbstractLayout;
import cytoscape.layout.CyLayoutAlgorithm;
import cytoscape.layout.CyLayouts;
import cytoscape.layout.LayoutProperties;
import cytoscape.layout.Tunable;

import cytoscape.plugin.CytoscapePlugin;
import cytoscape.util.CytoscapeAction;
import cytoscape.view.CyNetworkView;
import cytoscape.view.cytopanels.CytoPanelImp;

import cytoscape.visual.ArrowShape;
import cytoscape.visual.CalculatorCatalog;
import cytoscape.visual.EdgeAppearance;
import cytoscape.visual.EdgeAppearanceCalculator;
import cytoscape.visual.GlobalAppearanceCalculator;
import cytoscape.visual.LineStyle;
import cytoscape.visual.NodeAppearance;
import cytoscape.visual.NodeAppearanceCalculator;
import cytoscape.visual.NodeShape;
import cytoscape.visual.VisualMappingManager;
import cytoscape.visual.VisualPropertyDependency;
import cytoscape.visual.VisualPropertyType;
import cytoscape.visual.VisualStyle;
import cytoscape.visual.calculators.BasicCalculator;
import cytoscape.visual.calculators.Calculator;
import cytoscape.visual.mappings.BoundaryRangeValues;
import cytoscape.visual.mappings.ContinuousMapping;
import cytoscape.visual.mappings.DiscreteMapping;
import cytoscape.visual.mappings.Interpolator;
import cytoscape.visual.mappings.LinearNumberToColorInterpolator;
import cytoscape.visual.mappings.ObjectMapping;
import cytoscape.visual.mappings.PassThroughMapping;

import giny.model.GraphPerspective;
import giny.model.Node;
import giny.view.NodeView;
import java.io.*;
import java.lang.Math;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Iterator;
import java.util.Scanner;
import java.util.Set;
import java.util.Vector;
import java.util.HashMap;
import java.util.HashSet;
import java.util.TreeMap;
import java.awt.Color;
import java.awt.GridLayout;
import java.awt.event.ActionEvent;
import java.awt.geom.Point2D;
import javax.swing.AbstractAction;
import javax.swing.Action;
import javax.swing.ImageIcon;
import javax.swing.JButton;
import javax.swing.JFileChooser;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JSeparator;
import javax.swing.JSlider;
import javax.swing.JSpinner;
import javax.swing.JTextArea;
import javax.swing.SpinnerNumberModel;
import javax.swing.SwingConstants;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ChangeListener;

/**
 */
public class Expand extends CytoscapePlugin 
{
    private ImageIcon iconAdd  = new ImageIcon(getClass().getResource("/icon_add.png"));
    private ImageIcon iconPrev = new ImageIcon(getClass().getResource("/icon_prev.png"));
    private ImageIcon iconNext = new ImageIcon(getClass().getResource("/icon_next.png"));
    private String dbFile = null; // new String("/Users/jwazny/sample.db");
    private Database db = null;
    private HashMap nodeMap = new HashMap();
    private String vsName = "Expander Style"; 
    private Color spentNodeColor = new Color(0.9f, 0.9f, 0.9f);
    private Color normalNodeColor = Color.WHITE;
    private double nodeWidthScale = 100.0;
    private double nodeHeightScale = 5.0;

    private String versionStr = "20120525";

    public Expand() 
    {
        // Install user interface elements.
        ToolBarAction addButton = new AddButtonAction(this);
        ToolBarAction prevButton = new PrevButtonAction(this);
        ToolBarAction nextButton = new NextButtonAction(this);
        Cytoscape.getDesktop().getCyMenus().addCytoscapeAction(addButton);
        Cytoscape.getDesktop().getCyMenus().addCytoscapeAction(prevButton);
        Cytoscape.getDesktop().getCyMenus().addCytoscapeAction(nextButton);

        CytoPanelImp ctrlPanel = (CytoPanelImp) Cytoscape.getDesktop().getCytoPanel(SwingConstants.WEST);
        ConfigPanel configPanel = new ConfigPanel(this);
        ctrlPanel.add("Expander", configPanel);
        int cytoPanelIndex = ctrlPanel.indexOfComponent("Expand");
        ctrlPanel.setSelectedIndex(cytoPanelIndex);

        // Establish visual mapping.
        VisualMappingManager manager = Cytoscape.getVisualMappingManager();
        CalculatorCatalog catalog = manager.getCalculatorCatalog();
        VisualStyle vs = catalog.getVisualStyle(vsName);
        if (vs == null) {
                // if not, create it and add it to the catalog
                vs = createVisualStyle();
                catalog.addVisualStyle(vs);
        }

        AlignLayout alignLayout = new AlignLayout(this);
        JitterLayout jitterLayout = new JitterLayout(this);
        CyLayouts.addLayout(alignLayout, "Expander layouts");
        CyLayouts.addLayout(jitterLayout, "Expander layouts");
    }

    abstract class ToolBarAction extends CytoscapeAction 
    {
        public ToolBarAction(ImageIcon icon, Expand plugin, String toolTip)
        {
            super("", icon);
            this.putValue(Action.SHORT_DESCRIPTION, toolTip);
        }
    }

    public class PrevButtonAction extends ToolBarAction
    {
        public PrevButtonAction(Expand plugin)
        {
            super(iconPrev, plugin, "Reveal preceding nodes");
        }

        public void actionPerformed(ActionEvent e)
        {
            CyNetwork net = Cytoscape.getCurrentNetwork();
            Set nodes = net.getSelectedNodes();
            for (Iterator itr = nodes.iterator(); itr.hasNext(); )
            {
                try
                {
                    addPredecessors(net, (CyNode)itr.next());
                }
                catch (IOException ex)
                {
                    break;
                }
            }

            redraw();
        }

        public boolean isInToolBar() 
        {
            return true;
        }

        public boolean isInMenuBar() 
        {
            return false;
        }
    }

    public class NextButtonAction extends ToolBarAction
    {
        public NextButtonAction(Expand plugin)
        {
            super(iconNext, plugin, "Reveal following nodes");
        }

        public void actionPerformed(ActionEvent e)
        {
            CyNetwork net = Cytoscape.getCurrentNetwork();
            Set nodes = net.getSelectedNodes();
            for (Iterator itr = nodes.iterator(); itr.hasNext(); )
            {
                try
                {
                    addSuccessors(net, (CyNode)itr.next());
                }
                catch (IOException ex)
                {
                    break;
                }
            }

            redraw();
        }

        public boolean isInToolBar() 
        {
            return true;
        }

        public boolean isInMenuBar() 
        {
            return false;
        }
    }

    public class AddButtonAction extends ToolBarAction
    {
        public AddButtonAction(Expand plugin)
        {
            super(iconAdd, plugin, "Add a contig");
        }

        public void actionPerformed(ActionEvent e)
        {
            if (db == null)
            {
                return;
            }

            try
            {
                String id = (String)JOptionPane.showInputDialog(Cytoscape.getDesktop(), 
                                                                "Enter the id of the contig to add", "");
                int contig = Integer.parseInt(id);

                if (!db.isNode(contig))
                {
                    JOptionPane.showMessageDialog(Cytoscape.getDesktop(), "No such contig " + id);
                    return;
                }

                // Add the contig node.
                CyNetwork net = Cytoscape.getCurrentNetwork();
                CyNode node = Cytoscape.getCyNode(id, true);
                decorateAndAddNode(net, node, contig);
                addNodesEdges(net, node);

                redraw();
            }
            catch (IOException ex)
            {
                return;
            }
        }

        public boolean isInToolBar()
        {
            return true;
        }

        public boolean isInMenuBar()
        {
            return false;
        }
    }

    class ConfigPanel extends JPanel
    {
        public class FileButtonAction extends AbstractAction
        {
            public FileButtonAction(JLabel l)
            {
                super("Select database file");
                label = l;
            }

            public void actionPerformed(ActionEvent e)
            {
                JFileChooser chooser = new JFileChooser();
                int ret = chooser.showOpenDialog(Cytoscape.getDesktop());
                if (ret == JFileChooser.APPROVE_OPTION)
                {
                    // String file = chooser.getSelectedFile().getName();
                    String file = chooser.getSelectedFile().toString();
                    if (plugin.resetDb(file))
                    {
                        label.setText(file);
                    }
                }
            }

            private JLabel label;
        }

        public class NodeWidthScaleChangeListener implements ChangeListener
        {

            public void stateChanged(ChangeEvent e)
            {
                double scale = spinner.getNumber().doubleValue();
                plugin.updateNodeScale(scale);
                plugin.redraw();
            }

            public NodeWidthScaleChangeListener(Expand p, SpinnerNumberModel s)
            {
                plugin = p;
                spinner = s;
            }

            private Expand plugin;
            private SpinnerNumberModel spinner;
        }

        private JLabel  label;
        private JButton fileButton;
        private Expand plugin;

        public ConfigPanel(Expand p)
        {
            // super(new GridLayout(0, 1));
            plugin = p;
            label = new JLabel("No file");
            JButton fileButton = new JButton("Select file");
            fileButton.setAction(new FileButtonAction(label));
            SpinnerNumberModel nodeWidthScaleSpinMod = new SpinnerNumberModel(100.0, 1.0, 1000.0, 1.0);
            JSpinner nodeWidthScale = new JSpinner(nodeWidthScaleSpinMod);
            nodeWidthScale.addChangeListener(new NodeWidthScaleChangeListener(p, nodeWidthScaleSpinMod));
            add(label);
            add(fileButton);
            add(nodeWidthScale);
            add(new JSeparator(SwingConstants.HORIZONTAL)); // Doesn't appear to do anything.
            add(new JLabel(versionStr + " (Copyright NICTA 2012)"));
        }
    }

    public static void redraw()
    {
        CyNetworkView view = Cytoscape.getCurrentNetworkView();
        view.redrawGraph(true, true);
    }

    // Add all edges between the new node and existing nodes.
    public void addNodesEdges(CyNetwork net, CyNode node) throws IOException
    {
        if (db == null)
        {
            return;
        }

        assertSpentness(net, node);
        CyAttributes attrs = Cytoscape.getEdgeAttributes();
        int contig = Integer.parseInt(node.getIdentifier());
        int rc = db.reverseComplement(contig);
        Vector froms = db.linksFrom(contig);
        Vector tos = db.linksTo(contig);

        // Add linked node edges.
        for (Iterator itr = froms.iterator(); itr.hasNext(); )
        {
            Link link = (Link)itr.next();
            CyNode to = (CyNode)nodeMap.get(link.target);
            if (to != null)
            {
                assertSpentness(net, to);
                double width = calcEdgeWidth(link.count);
                CyEdge edge = Cytoscape.getCyEdge(node, to, Semantics.INTERACTION, "->", true);
                attrs.setAttribute(edge.getIdentifier(), "gap", link.gap);
                attrs.setAttribute(edge.getIdentifier(), "count", link.count);
                attrs.setAttribute(edge.getIdentifier(), "rc", false);
                attrs.setAttribute(edge.getIdentifier(), "width", width);
                net.addEdge(edge);
            }
        }

        for (Iterator itr = tos.iterator(); itr.hasNext(); )
        {
            Link link = (Link)itr.next();
            CyNode from = (CyNode)nodeMap.get(link.source);
            if (from != null)
            {
                assertSpentness(net, from);
                double width = calcEdgeWidth(link.count);
                CyEdge edge = Cytoscape.getCyEdge(from, node, Semantics.INTERACTION, "->", true);
                attrs.setAttribute(edge.getIdentifier(), "gap", link.gap);
                attrs.setAttribute(edge.getIdentifier(), "count", link.count);
                attrs.setAttribute(edge.getIdentifier(), "rc", false);
                attrs.setAttribute(edge.getIdentifier(), "width", width);
                net.addEdge(edge);
            }
        }

        // Add rc edge.
        CyNode rcNode = (CyNode)nodeMap.get(rc);
        if (rcNode != null)
        {
            CyEdge edge = Cytoscape.getCyEdge(node, rcNode, Semantics.INTERACTION, "<->", true);
            attrs.setAttribute(edge.getIdentifier(), "rc", true);
            attrs.setAttribute(edge.getIdentifier(), "width", 1.0);
            net.addEdge(edge);
        }
    }

    // Updates the given node's `spent' attribute.
    public void assertSpentness(CyNetwork net, CyNode node) throws IOException
    {
        boolean spent = true;
        int id = Integer.parseInt(node.getIdentifier());
        Vector tos = db.linksTo(id);

        for (Iterator itr = tos.iterator(); itr.hasNext(); )
        {
            Link link = (Link)itr.next();
            if (!nodeMap.containsKey(link.source))
            {
                spent = false;
                break;
            }
        }
        if (spent)
        {
            Vector froms = db.linksFrom(id);
            for (Iterator itr = froms.iterator(); itr.hasNext(); )
            {
                Link link = (Link)itr.next();
                if (!nodeMap.containsKey(link.target))
                {
                    spent = false;
                    break;
                }
            }
        }

        CyAttributes attrs = Cytoscape.getNodeAttributes();
        attrs.setAttribute(node.getIdentifier(), "spent", spent);
    }

    public void addSuccessors(CyNetwork net, CyNode node) throws IOException
    {
        if (db == null)
        {
            return;
        }

        int contig = Integer.parseInt(node.getIdentifier());
        Point2D pos = getNodePosition(node);
        Vector froms = db.linksFrom(contig);
        double dTheta = Math.PI / (2 * ((double)froms.size() + 1.0));
        double theta = Math.PI / 4.0;
        double width = nodeWidth(net, node);
        for (Iterator itr = froms.iterator(); itr.hasNext(); )
        {
            Link lnk = (Link)itr.next();
            CyNode succ = Cytoscape.getCyNode(Integer.toString(lnk.target), true);
            if (!nodeMap.containsKey(lnk.target))
            {
                decorateAndAddNode(net, succ, lnk.target);
                double succWidth = nodeWidth(net, succ);
                double dist = 50 + (width + succWidth) / 2.0;
                theta -= dTheta;
                Point2D.Double newPos = new Point2D.Double(pos.getX() + dist * Math.cos(theta), pos.getY() + dist * Math.sin(theta));
                setNodePosition(succ, newPos);
            }
        }
        for (Iterator itr = froms.iterator(); itr.hasNext(); )
        {
            Link lnk = (Link)itr.next();
            CyNode succ = Cytoscape.getCyNode(Integer.toString(lnk.target), true);
            addNodesEdges(net, succ);
        }
    }
        
    public void addPredecessors(CyNetwork net, CyNode node) throws IOException
    {
        if (db == null)
        {
            return;
        }

        int contig = Integer.parseInt(node.getIdentifier());
        Point2D pos = getNodePosition(node);
        Vector tos = db.linksTo(contig);
        double dTheta = Math.PI / (2 * ((double)tos.size() + 1.0));
        double theta = 5.0 * Math.PI / 4.0;
        double width = nodeWidth(net, node);
        for (Iterator itr = tos.iterator(); itr.hasNext(); )
        {
            Link lnk = (Link)itr.next();
            CyNode pred = Cytoscape.getCyNode(Integer.toString(lnk.source), true);
            if (!nodeMap.containsKey(lnk.source))
            {
                decorateAndAddNode(net, pred, lnk.source);
                double predWidth = nodeWidth(net, pred);
                double dist = 50 + (width + predWidth) / 2.0;
                theta -= dTheta;
                Point2D.Double newPos = new Point2D.Double(pos.getX() + dist * Math.cos(theta), pos.getY() + dist * Math.sin(theta));
                setNodePosition(pred, newPos);
            }
        }
        for (Iterator itr = tos.iterator(); itr.hasNext(); )
        {
            Link lnk = (Link)itr.next();
            CyNode pred = Cytoscape.getCyNode(Integer.toString(lnk.source), true);
            addNodesEdges(net, pred);
        }
    }

    public int nodeSeqLength(CyNetwork net, CyNode node)
    {
        CyAttributes attrs = Cytoscape.getNodeAttributes();
        return attrs.getIntegerAttribute(node.getIdentifier(), "seqLength");
    }

    public double nodeWidth(CyNetwork net, CyNode node)
    {
        CyAttributes attrs = Cytoscape.getNodeAttributes();
        return attrs.getDoubleAttribute(node.getIdentifier(), "width");
    }

    public double calcNodeWidth(int len)
    {
        double width = (double)len / nodeWidthScale;
        if (width < 1.0)
        {
            width = 1.0;
        }
        return width;
    }

    public double calcNodeHeight(double covMean)
    {
        if (covMean < 1.0)
        {
            return 10.0;
        }
        double height = Math.log(covMean) / Math.log(2.0) * nodeHeightScale;
        if (height < 1.0)
        {
            height = 1.0;
        }
        return height;
    }

    public void updateNodeScale(double scale)
    {
        if (scale <= 1.0)
        {
            scale = 1.0;
        }
        nodeWidthScale = scale;
        
        CyNetwork net = Cytoscape.getCurrentNetwork();
        CyAttributes attrs = Cytoscape.getNodeAttributes();
        for (Iterator it = nodeMap.entrySet().iterator(); it.hasNext(); )
        {
            Map.Entry pair = (Map.Entry)(it.next());
            CyNode node = (CyNode)pair.getValue();
            int len = attrs.getIntegerAttribute(node.getIdentifier(), "seqLength");
            double width = calcNodeWidth(len);
            attrs.setAttribute(node.getIdentifier(), "width", new Double(width));
        }
    }

    public double calcEdgeWidth(int count)
    {
        if (count <= 1)
        {
            count = 2;
        }
        double width = Math.log((double)count) / Math.log(2.0);
        return width / 2;
    }

    public void decorateAndAddNode(CyNetwork net, CyNode node, int id) throws IOException
    {
        if (db == null)
        {
            return;
        }

        long len = db.length(id);
        Alignment align = db.alignment(id);
        Coverage cov = db.coverage(id);
        int rc = db.reverseComplement(id);
        CyAttributes attrs = Cytoscape.getNodeAttributes();
        double width = calcNodeWidth((int)len);
        double height = calcNodeHeight(cov.mean);
        String nodeId = node.getIdentifier();
        attrs.setAttribute(nodeId, "width", new Double(width));
        attrs.setAttribute(nodeId, "height", new Double(height));
        attrs.setAttribute(nodeId, "seq", db.sequence(id));
        attrs.setAttribute(nodeId, "seqLength", new Integer((int)len));
        attrs.setAttribute(nodeId, "ref", align.target);
        attrs.setAttribute(nodeId, "refBegin", align.start);
        attrs.setAttribute(nodeId, "refEnd", align.end);
        attrs.setAttribute(nodeId, "rc", rc);
        attrs.setAttribute(nodeId, "spent", false);
        attrs.setAttribute(nodeId, "strand", align.strand);
        attrs.setAttribute(nodeId, "gene", align.gene);
        attrs.setAttribute(nodeId, "covMean", cov.mean);

        net.addNode(node);
        nodeMap.put(id, node);
    }

    public static void setNodePosition(CyNode node, Point2D pos)
    {
        CyNetworkView netView = Cytoscape.getCurrentNetworkView();
        NodeView nodeView = netView.getNodeView(node);
        nodeView.setXPosition(pos.getX());
        nodeView.setYPosition(pos.getY());
        nodeView.setNodePosition(true);
    }

    public static Point2D getNodePosition(CyNode node)
    {
        CyNetworkView netView = Cytoscape.getCurrentNetworkView();
        NodeView nodeView = netView.getNodeView(node);
        return new Point2D.Double(nodeView.getXPosition(), nodeView.getYPosition());
    }

    public CyNode getCyNode(int id)
    {
        CyNode node = (CyNode)nodeMap.get(id);
        if (node == null)
        {
            // JOptionPane.showMessageDialog(Cytoscape.getDesktop(), "No such contig " + id);
        }
        return node;
    }

    public CyNode getCyNode(String id)
    {
        int contig = Integer.parseInt(id);
        return getCyNode(contig);
    }

    public double getNodeWidthScale()
    {
        return nodeWidthScale;
    }

    public boolean resetDb(String dbFilename)
    {
        try 
        {
            Database newDb = new Database(dbFilename);
            dbFile = dbFilename;
            db = newDb;
            CyNetwork net = Cytoscape.getCurrentNetwork();
            Cytoscape.destroyNetwork(net);
            nodeMap.clear();
        }
        catch (Exception e)
        {
            JOptionPane.showMessageDialog(Cytoscape.getDesktop(), "Failed to load file: " + e);
            return false;
        }
        return true;
    }

    public class Link
    {
        public Link(int s, int t, int c, int g)
        {
            source = s;
            target = t;
            count = c;
            gap = g;
        }

        public int source;
        public int target;
        public int count;
        public int gap;
    }

    public class Alignment
    {
        public Alignment(int c, String t, int a, int b, int m, String s, String g)
        {
            id = c;
            target = t;
            start = a;
            end = b;
            matched = m;
            strand = s;
            gene = g;
        }
    
        public int id;
        public String target;
        public int start;
        public int end;
        public int matched;
        public String strand;
        public String gene;
    }

    public class Coverage
    {
        public Coverage(double m)
        {
            mean = m;
        }

        public double mean;
    }

    public class Database
    {
        public int reverseComplement(int node) throws IOException
        {
            long ofs = fileOffset(node);
            file.seek(ofs);
            long rc = file.readLong();
            return (int)rc;
        }

        public Vector linksTo(int node) throws IOException
        {
            Vector result = new Vector();
            long ofs = fileOffset(node);
            file.seek(ofs + 8 + 8);
            long numFwd = file.readLong();
            long numBck = file.readLong();
            file.seek(ofs + 8 + 8 + 8 + 8 + numFwd * 24);
            for (long i = 0; i < numBck; ++i)
            {
                long source = file.readLong();
                long gap = file.readLong();
                long count = file.readLong();
                result.add(new Link((int)source, node, (int)count, (int)gap));
            }
            return result;
        }

        public Vector linksFrom(int node) throws IOException
        {
            Vector result = new Vector();
            long ofs = fileOffset(node);
            file.seek(ofs + 8 + 8);
            long numFwd = file.readLong();
            long numBck = file.readLong();
            for (long i = 0; i < numFwd; ++i)
            {
                long target = file.readLong();
                long gap = file.readLong();
                long count = file.readLong();
                result.add(new Link(node, (int)target, (int)count, (int)gap));
            }
            return result;
        }

        public Alignment alignment(int node) throws IOException
        {
            long ofs = fileOffset(node);
            file.seek(ofs + 8);
            long len = file.readLong();
            long numFwd = file.readLong();
            long numBck = file.readLong();
            file.seek(ofs + 8 + 8 + 8 + 8 + 24 * numFwd + 24 * numBck + len);
            
            long targetLen = file.readLong();
            byte[] targetBytes = new byte[(int)targetLen];
            long preTargetOfs = file.getFilePointer();
            file.readFully(targetBytes, 0, (int)targetLen);
            long postTargetOfs = file.getFilePointer();
            String target = new String(targetBytes, "UTF-8");
            long start = file.readLong();
            long end = file.readLong();
            long matched = file.readLong();
            String strand = "?";
            String gene = "??";
            long preStrandOfs = file.getFilePointer();
            if (magic >= magic101)
            {
                byte[] strandBytes = new byte[1];
                strandBytes[0] = file.readByte();
                strand = new String(strandBytes, "UTF-8");
                
                long postStrandOfs = file.getFilePointer();

                long geneLen = file.readLong();
                byte[] geneBytes = new byte[(int)geneLen];
                file.readFully(geneBytes, 0, (int)geneLen);
                gene = new String(geneBytes, "UTF-8");
                long postGeneOfs = file.getFilePointer();
/*
                JOptionPane.showMessageDialog(Cytoscape.getDesktop(), "ofs: " + ofs + ", targetLen: " + targetLen + ", start: " + start + ", end: " + end + ", geneLen: " + geneLen +
                                                                       ", preTargetOfs: " + preTargetOfs + ", postTargetOfs: " + postTargetOfs + 
                                                                       ", preStrandOfs: " + preStrandOfs + ", postStrandOfs: " + postStrandOfs + ", postGeneOfs: " + postGeneOfs);
*/
            }
            return new Alignment(node, target, (int)start, (int)end, (int)matched, strand, gene);
        }

        public Coverage coverage(int node) throws IOException
        {
            if (magic < magic101)
            {
                return new Coverage(0.0);
            }

            long ofs = fileOffset(node);
            file.seek(ofs + 8);
            long len = file.readLong();
            long numFwd = file.readLong();
            long numBck = file.readLong();
            long alnOfs = ofs + 8 + 8 + 8 + 8 + 24 * numFwd + 24 * numBck + len;
            file.seek(alnOfs);
            
            long nameLen = file.readLong();
            long geneOfs = alnOfs + 8 + nameLen + 8 + 8 + 8 + 1;
            file.seek(geneOfs);
            long geneLen = file.readLong();
            file.seek(geneOfs + 8 + geneLen);
            long postGeneOfs = file.getFilePointer();

            // At coverage.
            double mean = file.readDouble();
            return new Coverage(mean);
        }

        public boolean isNode(int node) throws IOException
        {
            return length(node) > 0;
        }

        public long length(int node) throws IOException
        {
            if (node >= numEntries)
            {
                return 0;
            }
            long ofs = fileOffset(node);
            file.seek(ofs + 8);
            long len = file.readLong();
            return len;
        }

        public String sequence(int node) throws IOException
        {
            if (node >= numEntries)
            {
                return "";
            }
            long ofs = fileOffset(node);
            file.seek(ofs + 8);
            long len = file.readLong();
            long numFwd = file.readLong();
            long numBck = file.readLong();
            file.seek(ofs + 8 + 8 + 8 + 8 + 24 * numFwd + 24 * numBck);
            byte[] seq = new byte[(int)len];
            file.readFully(seq, 0, (int)len);
            return new String(seq, "UTF-8");
        }

        public long fileOffset(int node) throws IOException
        {
            if (node >= numEntries)
            {
                // TODO: Not this!
                return 8 + 8;
            }

            // magic + numEntries + TOC offset
            file.seek(8 + 8 + 8 * node);
            long ofs = file.readLong();
            return ofs;
        }

        public long numEntries()
        {
            return numEntries;
        }

        public Database(String dbFile) throws Exception,IOException
        {
            file = new RandomAccessFile(dbFile, "r");
            long m = file.readLong();
            if (m != magic100 && m != magic101)
            {
                throw new Exception("Invalid input file: " + m);
            }
            magic = m;
            numEntries = file.readLong();
        }

        private long magic;
        private long magic100 = 5135597005161181232L;  // "GENDB100" (big endian uint64_t)
        private long magic101 = 5135597005161181233L;  // "GENDB101" (big endian uint64_t)
        private long numEntries;
        private RandomAccessFile file;
    }

    VisualStyle createVisualStyle()
    {

        // Create the visual style 
        VisualStyle style = new VisualStyle(vsName);

        NodeAppearanceCalculator nodeAppCalc = style.getNodeAppearanceCalculator();
        EdgeAppearanceCalculator edgeAppCalc = style.getEdgeAppearanceCalculator();
        GlobalAppearanceCalculator globalAppCalc = style.getGlobalAppearanceCalculator(); 

        // Rectangle nodes.
        NodeAppearance nodeApp = nodeAppCalc.getDefaultAppearance();
        nodeApp.set(VisualPropertyType.NODE_SHAPE, NodeShape.RECT);
        nodeApp.set(VisualPropertyType.NODE_WIDTH, 50.0);

        // Smaller node font.
        nodeApp.set(VisualPropertyType.NODE_FONT_SIZE, 10.0);

        // Slightly transparent nodes.
        nodeApp.set(VisualPropertyType.NODE_OPACITY, 200.0);

        // Variable node width.
        PassThroughMapping passWidth = new PassThroughMapping(Double.class, "width");
        Calculator nodeWidthCalc     = new BasicCalculator("Node Width Calculator", 
                                                           passWidth, VisualPropertyType.NODE_WIDTH);
        nodeAppCalc.setCalculator(nodeWidthCalc);

        PassThroughMapping passHeight = new PassThroughMapping(Double.class, "height");
        Calculator nodeHeightCalc     = new BasicCalculator("Node Height Calculator",
                                                            passHeight, VisualPropertyType.NODE_HEIGHT);
        nodeAppCalc.setCalculator(nodeHeightCalc);

        DiscreteMapping disColor = new DiscreteMapping(Color.class, "spent");
        disColor.putMapValue(new Boolean(false), normalNodeColor);
        disColor.putMapValue(new Boolean(true), spentNodeColor);
        Calculator nodeColorCalc = new BasicCalculator("Node Color Calculator",
                                                       disColor, VisualPropertyType.NODE_FILL_COLOR);
        nodeAppCalc.setCalculator(nodeColorCalc);

        // Node label.
        PassThroughMapping passId = new PassThroughMapping(String.class, "gene");
        Calculator nodeLabelCalc  = new BasicCalculator("Node Label Calculator", 
                                                        passId, VisualPropertyType.NODE_LABEL);
        nodeAppCalc.setCalculator(nodeLabelCalc);

        // Edge arrows.
        EdgeAppearance edgeApp = edgeAppCalc.getDefaultAppearance();
        edgeApp.set(VisualPropertyType.EDGE_TGTARROW_SHAPE, ArrowShape.DELTA);

        DiscreteMapping disSrcArrow = new DiscreteMapping(ArrowShape.class, "rc");
        disSrcArrow.putMapValue(new Boolean(false), ArrowShape.NONE);
        disSrcArrow.putMapValue(new Boolean(true),  ArrowShape.DELTA);
        Calculator srcArrowCalc = new BasicCalculator("Source Arrow Calculator",
                                                       disSrcArrow, VisualPropertyType.EDGE_SRCARROW_SHAPE);
        edgeAppCalc.setCalculator(srcArrowCalc);

        // Edge line style.
        DiscreteMapping disEdgeStyle = new DiscreteMapping(LineStyle.class, "rc");
        disEdgeStyle.putMapValue(new Boolean(false), LineStyle.SOLID);
        disEdgeStyle.putMapValue(new Boolean(true), LineStyle.LONG_DASH);
        Calculator edgeStyleCalc = new BasicCalculator("Edge Style Calculator",
                                                       disEdgeStyle, VisualPropertyType.EDGE_LINE_STYLE);
        edgeAppCalc.setCalculator(edgeStyleCalc);

        // Edge width.
        PassThroughMapping passEdgeWidth = new PassThroughMapping(Double.class, "width");
        Calculator edgeWidthCalc = new BasicCalculator("Edge Width Calculator", 
                                                       passEdgeWidth, VisualPropertyType.EDGE_LINE_WIDTH);
        edgeAppCalc.setCalculator(edgeWidthCalc);

        // Decouple node width/height.
        style.getDependency().set(VisualPropertyDependency.Definition.NODE_SIZE_LOCKED, false);

        return style;
    }

    // Stratifies nodes vertically based on reference, then positions horizontally according to alignment coords.
    class AlignLayout extends AbstractLayout
    {
        Expand plugin;
        double hierGap = -200.0;
        double strandGap = 25;
        double maxEdgeLen = 200.0;

        public AlignLayout(Expand p)
        {
            super();
            plugin = p;
        }

        public String getName() 
        {
            return "Expander alignment layout";
        }

        public String toString()
        {
            return "Expander alignment layout";
        }

        public void construct()
        {
            // taskMonitor.setStatus("initializing");
            initialize();
	    
            // Group nodes by reference (chromosome), then order those by reference start.
            // (Scale distances by nodeWidthScale.)
            HashMap<String, TreeMap<Integer, List<CyNode>>> refMap = new HashMap<String, TreeMap<Integer, List<CyNode>>>();
            CyAttributes attrs = Cytoscape.getNodeAttributes();

            // Accumulate all nodes by reference.
            Set todo = new HashSet();
            Iterator<Node> nodeItr = network.nodesIterator();
            while (nodeItr.hasNext()) 
            {
                if (canceled)
                    return;

                String id = ((Node)nodeItr.next()).getIdentifier();
                int contig = Integer.parseInt(id);
                CyNode node = plugin.getCyNode(contig);
                String ref = attrs.getStringAttribute(id, "ref");
                if (ref.equals(""))
                {
                    todo.add(node);
                    continue;
                }
                Integer refBegin = attrs.getIntegerAttribute(id, "refBegin");
                if (!refMap.containsKey(ref))
                {
                    refMap.put(ref, new TreeMap<Integer, List<CyNode>>());
                }
                TreeMap<Integer, List<CyNode>> ofsMap = refMap.get(ref);
                if (!ofsMap.containsKey(refBegin))
                {
                    ofsMap.put(refBegin, new ArrayList<CyNode>());
                }
                refMap.get(ref).get(refBegin).add(node);
            }

            // Place nodes aligned to each reference on the same stratum.
            double nodeWidthScale = plugin.getNodeWidthScale();
            double y = 0.0;
            int refX = 0;
            double ofsX = 0.0;
            double prevX = 0.0;
            Iterator refItr = refMap.entrySet().iterator();
            while (refItr.hasNext())
            {
                ofsX = 0.0;
                Map.Entry refPair = (Map.Entry)refItr.next();
                TreeMap<Integer, List<CyNode>> refContigs = (TreeMap<Integer, List<CyNode>>)refPair.getValue();
                if (refContigs == null || refContigs.size() == 0)
                {
                    continue;
                }
 
                String ref = (String)refPair.getKey();
                refX = refContigs.firstKey();
                Iterator contigsItr = refContigs.entrySet().iterator();
                while (contigsItr.hasNext())
                {
                    Map.Entry pair = (Map.Entry)contigsItr.next();
                    Integer pos = (Integer)pair.getKey();
                    List<CyNode> nodeList = (List<CyNode>)pair.getValue();

                    Iterator contigItr = nodeList.iterator();
                    while (contigItr.hasNext())
                    {
                        CyNode node = (CyNode)contigItr.next();
                        String id = node.getIdentifier();
                        double x = ((double)(pos - refX)) / nodeWidthScale + ofsX;
                        if (x - prevX > maxEdgeLen)
                        {
                            x = prevX + maxEdgeLen;
                            refX = pos;
                            ofsX = x;
                        }

                        double w = plugin.nodeWidth(network, node);
                        // Translate centre so that start lines up with calculated x.
                        x += w / 2.0;
                        // Offset according to strand - to separate rcs.
                        double yOfs = (attrs.getStringAttribute(id, "strand").equals("+") ? 1 : -1) * strandGap;
                        networkView.getNodeView(node).setXPosition(x);
                        networkView.getNodeView(node).setYPosition(y + yOfs);
                        prevX = x;
                    }
                }

                // Jump to another line.
                y += hierGap;
            }
                
            // Place the unaligned contigs.
            if (!todo.isEmpty())
            {
                setPositions(todo);
            }

            // TODO: Calculate an x offset for each reference which minimises the length of inter-reference edges!
        }

        private boolean setPosition(Set<CyNode> todo, Set<CyNode> seen, CyNode node, Point2D.Double pos)
        {
            if (!todo.contains(node))
            {
                NodeView view = networkView.getNodeView(node);
                pos.setLocation(view.getXPosition(), view.getYPosition());
                return true;
            }

            if (seen.contains(node))
            {
                return false;
            }
            seen.add(node);

            int contig = Integer.parseInt(node.getIdentifier());
            Vector froms;
            Vector tos;
            try
            {
                froms = db.linksFrom(contig);
                tos = db.linksTo(contig);
            }
            catch (IOException ex)
            {
                froms = new Vector();
                tos = new Vector();
            }
            for (Iterator itr = froms.iterator(); itr.hasNext(); )
            {
                Link link = (Link)itr.next();
                Point2D.Double toPos = new Point2D.Double();
                CyNode toNode = plugin.getCyNode(link.target);
                if (toNode != null && setPosition(todo, seen, toNode, toPos))
                {
                    double x = toPos.x - link.gap;
                    double y = toPos.y;
                    pos.setLocation(x, y);
                    NodeView view = networkView.getNodeView(node);
                    view.setXPosition(x);
                    view.setYPosition(y);
                    todo.remove(node);
                    return true;
                }
            }

            for (Iterator itr = tos.iterator(); itr.hasNext(); )
            {
                Link link = (Link)itr.next();
                Point2D.Double fromPos = new Point2D.Double();
                CyNode fromNode = plugin.getCyNode(link.source);
                if (fromNode != null && setPosition(todo, seen, fromNode, fromPos))
                {
                    double x = fromPos.x + link.gap;
                    double y = fromPos.y;
                    pos.setLocation(x, y);
                    NodeView view = networkView.getNodeView(node);
                    view.setXPosition(x);
                    view.setYPosition(y);
                    todo.remove(node);
                    return true;
                }
            }

            // No path to an aligned contig!
            return false;
        }

        private void setPositions(Set<CyNode> todo)
        {
            double y = -hierGap;
            while (!todo.isEmpty())
            {
                CyNode node = (CyNode)todo.iterator().next();
                Point2D.Double pos = new Point2D.Double();
                HashSet seen = new HashSet();
                if (!setPosition(todo, seen, node, pos))
                {
                    // No path to a positioned contig!
                    networkView.getNodeView(node).setXPosition(0);
                    networkView.getNodeView(node).setYPosition(y);
                    todo.remove(node);
                    y -= hierGap;
                }
            }
        }

    }

    class JitterLayout extends AbstractLayout
    {
        Expand plugin;

        public JitterLayout(Expand p)
        {
            super();
            plugin = p;
        }

        public  String getName() 
        {
            return "Expander jitter layout";
        }

        public  String toString()
        {
            return "Expander jitter layout";
        }

        public void construct()
        {
            Iterator<Node> nodeItr = network.nodesIterator();
            while (nodeItr.hasNext()) 
            {
                Node node = (Node)nodeItr.next();
                NodeView view = networkView.getNodeView(node);
                double y = view.getYPosition();
                y += 20.0 * Math.random() - 10.0;
                view.setYPosition(y);
            }
        }

    }

}
