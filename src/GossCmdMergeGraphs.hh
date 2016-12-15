// Copyright (c) 2008-1016, NICTA (National ICT Australia).
// Copyright (c) 2016, Commonwealth Scientific and Industrial Research
// Organisation (CSIRO) ABN 41 687 119 230.
//
// Licensed under the CSIRO Open Source Software License Agreement;
// you may not use this file except in compliance with the License.
// Please see the file LICENSE, included with this distribution.
//
#ifndef GOSSCMDMERGEGRAPHS_HH
#define GOSSCMDMERGEGRAPHS_HH

#ifndef GOSSCMDMERGE_HH
#include "GossCmdMerge.hh"
#endif

#ifndef GRAPH_HH
#include "Graph.hh"
#endif

typedef GossCmdMerge<Graph> GossCmdMergeGraphs;
typedef GossCmdFactoryMerge<Graph> GossCmdFactoryMergeGraphs;

#endif // GOSSCMDMERGEGRAPHS_HH
