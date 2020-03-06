#ifndef GRAPH_H
#define GRAPH_H

#include <algorithm>
#include <stack>
#include <cassert>
#include <limits>
#include <vector>

/*
 *  Compute a maximal independent set for a graph stored in CSR format
 *  using a greedy serial algorithm
 *
 *  Parameters
 *      num_rows   - number of rows in A (number of vertices)
 *      Ap[]       - CSR row pointer
 *      Aj[]       - CSR index array
 *      active     - value used for active vertices        (input)
 *       C         - value used to mark non-MIS vertices   (output)
 *       F         - value used to mark MIS vertices       (output)
 *      x[]        - state of each vertex
 *
 *
 *  Returns:
 *      The number of nodes in the MIS.
 *
 *  Notes:
 *      Only the vertices with values with x[i] == active are considered
 *      when determining the MIS.  Upon return, all active vertices will
 *      be assigned the value C or F depending on whether they are in the
 *      MIS or not.
 *
 */
template<class I, class T>
I maximal_independent_set_serial(const I num_rows,
                                 const I Ap[], const int Ap_size,
                                 const I Aj[], const int Aj_size,
                                 const T active,
                                 const T  C,
                                 const T  F,
                                       T  x[], const int  x_size)
{
    I N = 0;

    for(I i = 0; i < num_rows; i++){
        if(x[i] != active) continue;

        x[i] = C;
        N++;

        for(I jj = Ap[i]; jj < Ap[i+1]; jj++){
            const I j = Aj[jj];
            if(x[j] == active) {
                x[j] = F;
            }
        }

    }

    return N;
}

/*
 *  Compute a maximal independent set for a graph stored in CSR format
 *  using a variant of Luby's parallel MIS algorithm
 *
 *  Parameters
 *      num_rows   - number of rows in A (number of vertices)
 *      Ap[]       - CSR row pointer
 *      Aj[]       - CSR index array
 *      active     - value used for active vertices        (input)
 *       C         - value used to mark non-MIS vertices   (output)
 *       F         - value used to mark MIS vertices       (output)
 *      x[]        - state of each vertex
 *      y[]        - random values for each vertex
 *      max_iters  - maximum number of iterations
 *                   by default max_iters=-1 and no limit
 *                   is imposed
 *
 *  Returns:
 *      The number of nodes in the MIS.
 *
 *  Notes:
 *      Only the vertices with values with x[i] == active are considered
 *      when determining the MIS.  Upon return, all active vertices will
 *      be assigned the value C or F depending on whether they are in the
 *      MIS or not.
 *
 */
template<class I, class T, class R>
I maximal_independent_set_parallel(const I num_rows,
                                   const I Ap[], const int Ap_size,
                                   const I Aj[], const int Aj_size,
                                   const T active,
                                   const T  C,
                                   const T  F,
                                         T  x[], const int  x_size,
                                   const R  y[], const int  y_size,
                                   const I  max_iters)
{
    I N = 0;
    I num_iters = 0;

    bool active_nodes = true;

    while(active_nodes && (max_iters == -1 || num_iters < max_iters)){
        active_nodes = false;

        num_iters++;

        for(I i = 0; i < num_rows; i++){
            const R yi = y[i];

            if(x[i] != active) continue;

            const I row_start = Ap[i];
            const I row_end   = Ap[i+1];

            I jj;

            for(jj = row_start; jj < row_end; jj++){
                const I j  = Aj[jj];
                const T xj = x[j];

                if(xj == C) {
                    x[i] = F;                      //neighbor is MIS
                    break;
                }

                if(xj == active){
                    const R yj = y[j];
                    if(yj > yi)
                        break;                     //neighbor is larger
                    else if (yj == yi && j > i)
                        break;                     //tie breaker goes to neighbor
                }
            }

            if(jj == row_end){
                for(jj = row_start; jj < row_end; jj++){
                    const I j  = Aj[jj];
                    if(x[j] == active)
                        x[j] = F;
                }
                N++;
                x[i] = C;
            } else {
                active_nodes = true;
            }
        }
    } // end while

    //std::cout << std::endl << "Luby's finished in " << num_iters << " iterations " << std::endl;

    return N;
}

/*
 *  Compute a vertex coloring for a graph stored in CSR format.
 *
 *  The coloring is computed by removing maximal independent sets
 *  of vertices from the graph.  Specifically, at iteration i an
 *  independent set of the remaining subgraph is constructed and
 *  assigned color i.
 *
 *  Returns the K, the number of colors used in the coloring.
 *  On return x[i] \in [0,1, ..., K - 1] will contain the color
 *  of the i-th vertex.
 *
 */
template<class I, class T>
T vertex_coloring_mis(const I num_rows,
                      const I Ap[], const int Ap_size,
                      const I Aj[], const int Aj_size,
                            T  x[], const int  x_size)
{
    std::fill( x, x + num_rows, -1);

    I N = 0;
    T K = 0;

    while(N < num_rows){
        N += maximal_independent_set_serial(num_rows,Ap,Ap_size,Aj,Aj_size,-1-K,K,-2-K,x,x_size);
        K++;
    }

    return K;
}


/*
 *  Applies the first fit heuristic to a graph coloring.
 *
 *  For each vertex with color K the vertex is assigned the *first*
 *  available color such that no neighbor of the vertex has that
 *  color.  This heuristic is used to reduce the number of color used
 *  in the vertex coloring.
 *
 */
template<class I, class T>
void vertex_coloring_first_fit(const I num_rows,
                               const I Ap[], const int Ap_size,
                               const I Aj[], const int Aj_size,
                                     T  x[], const int  x_size,
                               const T  K)
{
    for(I i = 0; i < num_rows; i++){
        if(x[i] != K) continue;
        std::vector<bool> mask(K,false);
        for(I jj = Ap[i]; jj < Ap[i+1]; jj++){
            const I j = Aj[jj];
            if(  i == j  ) continue; //ignore diagonal
            if( x[j] < 0 ) continue; //ignore uncolored vertices
            mask[x[j]] = true;
        }
        x[i] = std::find(mask.begin(), mask.end(), false) - mask.begin();
    }
}



/*
 * Compute a vertex coloring of a graph using the Jones-Plassmann algorithm
 *
 *  Parameters
 *      num_rows   - number of rows in A (number of vertices)
 *      Ap[]       - CSR row pointer
 *      Aj[]       - CSR index array
 *      x[]        - color of each vertex
 *      y[]        - initial random values for each vertex
 *
 *  Notes:
 *      Arrays x and y will be overwritten
 *
 *  References:
 *      Mark T. Jones and Paul E. Plassmann
 *      A Parallel Graph Coloring Heuristic
 *      SIAM Journal on Scientific Computing 14:3 (1993) 654--669
 *      http://citeseer.ist.psu.edu/jones92parallel.html
 *
 */
template<class I, class T, class R>
T vertex_coloring_jones_plassmann(const I num_rows,
                                  const I Ap[], const int Ap_size,
                                  const I Aj[], const int Aj_size,
                                        T  x[], const int  x_size,
                                        R  z[], const int  z_size)
{
    std::fill( x, x + num_rows, -1);

    for(I i = 0; i < num_rows; i++){
        z[i] += Ap[i+1] - Ap[i];
    }

    I N = 0;
    T K = 0; //iteration number

    while(N < num_rows){
        N += maximal_independent_set_parallel(num_rows,Ap,Ap_size,Aj,Aj_size,-1,K,-2,x,x_size,z,z_size,1);
        for(I i = 0; i < num_rows; i++){
            if(x[i] == -2)
                x[i] = -1;
        }
        vertex_coloring_first_fit(num_rows,Ap,Ap_size,Aj,Aj_size,x,x_size,K);
        K++;
    }

    return *std::max_element(x, x + num_rows);
}


/*
 * Compute a vertex coloring of a graph using the parallel
 * Largest-Degree-First (LDF) algorithm
 *
 *  Parameters
 *      num_rows   - number of rows in A (number of vertices)
 *      Ap[]       - CSR row pointer
 *      Aj[]       - CSR index array
 *      x[]        - color of each vertex
 *      y[]        - initial random values for each vertex
 *
 *   References:
 *     J. R. Allwright and R. Bordawekar and P. D. Coddington and K. Dincer and C. L. Martin
 *     A Comparison of Parallel Graph Coloring Algorithms
 *     DRAFT SCCS-666
 *     http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.45.4650
 *
 */
template<class I, class T, class R>
T vertex_coloring_LDF(const I num_rows,
                      const I Ap[], const int Ap_size,
                      const I Aj[], const int Aj_size,
                            T  x[], const int  x_size,
                      const R  y[], const int  y_size)
{
    std::fill( x, x + num_rows, -1);

    std::vector<R> weights(num_rows);

    I N = 0;
    T K = 0; //iteration number

    while(N < num_rows){
        // weight is # edges in induced subgraph + random value
        for(I i = 0; i < num_rows; i++){
            if(x[i] != -1) continue;
            I num_neighbors = 0;
            for(I jj = Ap[i]; jj < Ap[i+1]; jj++){
                I j = Aj[jj];
                if(x[j] == -1 && i != j)
                    num_neighbors++;
            }
            weights[i] = y[i] + num_neighbors;
        }

        N += maximal_independent_set_parallel(num_rows,Ap,Ap_size,Aj,Aj_size,-1,K,-2,x,x_size,&weights[0],num_rows,1);
        for(I i = 0; i < num_rows; i++){
            if(x[i] == -2)
                x[i] = -1;
        }
        vertex_coloring_first_fit(num_rows,Ap,Ap_size,Aj,Aj_size,x,x_size,K);
        K++;
    }

    return *std::max_element(x, x + num_rows);
}


/*
 * Compute the incidence matrix for a clustering
 *
 *      I = Incidence matrix between nodes and clusters (num_nodes x num_clusters)
 * I[i,a] = 1 if node i is in cluster a, otherwise 0
 *
 *     Cluster indexes: a,b,c in 1..num_clusters
 * Global node indexes: i,j,k in 1..num_rows
 *  Local node indexes: pair (a,m) where a is cluster and m in 1..num_nodes_in_cluster
 *
 * We store I in both CSC and CSR formats because we want to be able
 * to map global <-> local node indexes. However, I in CSR format is
 * simply the cm array, so we only need to compute CSC format.
 *
 * IC = (ICp,ICi)    = I in CSC format (don't store ICx because it's
 *                     always 1).
 *
 * IR = (IRa) = (cm) = I in CSR format (don't store IRp because we
 *                     have exactly one nonzero entry per row, and
 *                     don't store IRx because it's always 1). This is
 *                     just the cm array.
 *
 * Converting local (a,m) -> global i:   i = ICi[ICp[a] + m]
 *
 * Converting global i -> local (a,m):   a = cm[i]
 *                                       m = L[i]
 *
 * L is an additional vector (length num_rows) to store local indexes.
 *
 *  Parameters
 *      num_nodes         - (IN)  number of nodes
 *      num_clusters      - (IN)  number of clusters
 *     cm[num_nodes]      - (IN)  cluster index for each node
 *    ICp[num_clusters+1] - (OUT) CSC column pointer array for I
 *    ICi[num_nodes]      - (OUT) CSC column indexes for I
 *      L[num_nodes]      - (OUT) Local index mapping
 */
template<class I>
void cluster_node_incidence(const I num_nodes,
                            const I num_clusters,
                            const I  cm[], const int  cm_size,
                                  I ICp[], const int ICp_size,
                                  I ICi[], const int ICi_size,
                                  I   L[], const int L_size)
{
    // Construct the ICi index array. It will have one entry per node,
    // giving the global node number, ordered so that all of the
    // cluster-0 nodes are first, then all the cluster-1 nodes, etc.
    for(I i = 0; i < num_nodes; i++){
        ICi[i] = i;
    }

    std::sort(ICi, ICi + ICi_size,
              [&](const I& i, const I& j)
              {
                  // sort first by cluster number, then by global node number
                  // within a cluster
                  return (cm[i] > cm[j]) || (cm[i] == cm[j] && i > j);
              }
             );

    // Construct the ICp pointer array. This code assumes that every
    // cluster contains at least one node (the cluster center).
    ICp[0] = 0;
    I a = 0; // current cluster
    for(I i = 0; i < num_nodes; i++){
        if(cm[ICi[i]] != a){
            a++;
            assert(a < num_clusters);
            ICp[a] = i;
        }
    }
    a++;
    assert(a == num_clusters); // all clusters were found
    ICp[a] = num_nodes;

    // Construct the local mapping vector L
    I i = 0; // global node number
    for(I a = 0; a < num_clusters; a++){
        const I N = ICp[a+1] - ICp[a]; // cluster size
        for(I m = 0; m < N; m++){
            L[i] = m;
            i++;
            assert(i <= num_nodes);
        }
    }
    assert(i == num_nodes); // all nodes were assigned

    ////////////////////////////////////////////////////////////
    // asserts below for testing, could be deleted

    // check that global -> local has a correct inverse
    I m;
    for(I i = 0; i < num_nodes; i++){
        a = cm[i];
        m = L[a];
        assert(a >= 0 && a < num_clusters);
        assert(m >= 0 && m < ICp[a+1] - ICp[a]);
        assert(i == ICi[ICp[a] + m]);
    }

    // check that local -> global has a correct inverse
    I j;
    for(I a = 0; a < num_clusters; a++){
        I N = ICp[a]; // cluster size
        for(I m = 0; m < N; m++){
            j = ICi[ICp[a] + m];
            assert(j >= 0 && j < num_nodes);
            assert(a == cm[j]);
            assert(m == L[j]);
        }
    }
}


/*
 * Apply Floyd–Warshall to cluster "a" and use the result to find the
 * cluster center
 *
 *  Parameters
 *      a                  - (IN) cluster index to find the center of
 *      num_nodes          - (IN) number of nodes
 *      num_clusters       - (IN) number of clusters
 *      Ap[]               - (IN) CSR row pointer
 *      Aj[]               - (IN) CSR index array
 *      Ax[]               - (IN) CSR data array (edge lengths)
 *      cm[num_nodes]      - (IN) cluster index for each node
 *     ICp[num_clusters+1] - (IN) CSC column pointer array for I
 *     ICi[num_nodes]      - (IN) CSC column indexes for I
 *       L[num_nodes]      - (IN) Local index mapping
 *
 *  Returns
 *      i                  - global node index of center of cluster a
 *
 *  References:
 *      https://en.wikipedia.org/wiki/Graph_center
 *      https://en.wikipedia.org/wiki/Floyd–Warshall_algorithm
 *      https://en.wikipedia.org/wiki/Distance_(graph_theory)
 */
template<class I, class T>
I cluster_center(const I a,
                 const I num_nodes,
                 const I num_clusters,
                 const I  Ap[], const int  Ap_size,
                 const I  Aj[], const int  Aj_size,
                 const T  Ax[], const int  Ax_size,
                 const I  cm[], const int  cm_size,
                 const I ICp[], const int ICp_size,
                 const I ICi[], const int ICi_size,
                 const I   L[], const int   L_size)
{
    I N = ICp[a+1] - ICp[a]; // size of the cluster

    // pairwise distances between all nodes in the cluster, in row-major order
    std::vector<T> dist(N*N, std::numeric_limits<T>::max());

    // Floyd-Warshall initialization
    for(I m = 0; m < N; m++){
        I i = ICi[ICp[a] + m]; // local node index (a,m) -> global i
        for(I jj = Ap[i]; jj < Ap[i+1]; jj++){ // all neighbors of i
            const I j = Aj[jj]; // neighbor global node index
            const T w = Ax[jj]; // edge weight

            if (cm[j] == a) { // only use neighbors in the same cluster
                const I n = L[j]; // global node index j -> local (a,n)
                assert(n >= 0 && n < N);
                const I mn = m*N + n; // row-major index of (m,n)
                dist[mn] = w;
            }
        }
        dist[m*N+m] = 0;
    }

    // main Floyd-Warshall iteration - O(N^3)
    for(I l = 0; l < N; l++){
        for(I m = 0; m < N; m++){
            for(I n = 0; n < N; n++){
                const I mn = m*N + n; // row-major index of (m,n)
                const I ml = m*N + l; // row-major index of (m,l)
                const I ln = l*N + n; // row-major index of (l,n)
                dist[mn] = std::min(dist[mn], dist[ml] + dist[ln]);
            }
        }
    }

    // check that the cluster is connected
    for(I i = 0; i < N*N; i++){
        assert(dist[i] < std::numeric_limits<T>::max());
    }

    // compute eccentricity of each node in cluster (max distance to other nodes)
    std::vector<T> ecc(N, 0);
    for(I m = 0; m < N; m++){
        for(I n = 0; n < N; n++){
            const I mn = m*N + n; // row-major index of (m,n)
            ecc[m] = std::max(ecc[m], dist[mn]);
        }
    }

    // graph center is the node with minimum eccentricity
    const I m = std::min_element(ecc.begin(), ecc.end()) - ecc.begin();
    const I i = ICi[ICp[a] + m]; // local node index (a,m) -> global i
    return i;
}


/*
 * Apply one iteration of Bellman-Ford iteration on a distance
 * graph stored in CSR format.
 *
 *  Parameters
 *      num_nodes - (IN)    number of nodes (number of rows in A)
 *      Ap[]      - (IN)    CSR row pointer
 *      Aj[]      - (IN)    CSR index array
 *      Ax[]      - (IN)    CSR data array (edge lengths)
 *      d[]       - (INOUT) distance to nearest center
 *     cm[]       - (INOUT) cluster index for each node
 *
 *  References:
 *      http://en.wikipedia.org/wiki/Bellman-Ford_algorithm
 */
template<class I, class T>
void bellman_ford(const I num_nodes,
                  const I Ap[], const int Ap_size,
                  const I Aj[], const int Aj_size,
                  const T Ax[], const int Ax_size,
                        T  d[], const int  d_size,
                        I cm[], const int cm_size)
{
    for(I i = 0; i < num_nodes; i++){
        T xi = d[i];
        I nci = cm[i];
        for(I jj = Ap[i]; jj < Ap[i+1]; jj++){
            const I j = Aj[jj];
            const T dd = Ax[jj] + d[j];
            if(dd < xi){
                xi = dd;
                nci = cm[j];
            }
        }
        d[i] = xi;
        cm[i] = nci;
    }
}


/*
 * Apply Bellman-Ford with a heuristic to balance cluster sizes
 *
 * This version is modified to break distance ties by assigning nodes
 * to the cluster with the fewest points, while preserving cluster
 * connectivity. This will hopefully result in more balanced cluster
 * sizes.
 *
 *  Parameters
 *     num_nodes    - (IN)    number of nodes (vertices)
 *     num_clusters - (IN)    number of clusters
 *     Ap[]         - (IN)    CSR row pointer for adjacency matrix A
 *     Aj[]         - (IN)    CSR index array
 *     Ax[]         - (IN)    CSR data array (edge lengths)
 *      d[]         - (INOUT) distance to nearest center
 *     cm[]         - (INOUT) cluster index for each node
 *      c[]         - (IN)    global node indexes of cluster centers
 *
 *  References:
 *      http://en.wikipedia.org/wiki/Bellman-Ford_algorithm
 */
template<class I, class T>
void bellman_ford_balanced(const I num_nodes,
                           const I num_clusters,
                           const I Ap[], const int Ap_size,
                           const I Aj[], const int Aj_size,
                           const T Ax[], const int Ax_size,
                                 T  d[], const int  d_size,
                                 I cm[], const int cm_size,
                                 I  c[], const int  c_size)
{
    assert(d_size == num_nodes);
    assert(cm_size == num_nodes);
    assert(c_size == num_clusters);

    std::vector<I> predecessor(num_nodes, -1); // index of predecessor node
    std::vector<I> pred_count(num_nodes, 0); // number of other nodes that we are the predecessor for

    std::vector<I> cs(num_clusters, 0); // cluster sizes (number of nodes in each cluster)
    for(I i = 0; i < num_nodes; i++){
        if(cm[i] > -1){
            cs[cm[i]]++;
        }
    }

    bool change; // did we make any changes during this iteration?
    I iteration = 0; // iteration count for safety check

    do{
        change = false;
        for(I i = 0; i < num_nodes; i++){
            for(I jj = Ap[i]; jj < Ap[i+1]; jj++){ // all neighbors of node i
                const I j = Aj[jj];
                const T new_d = Ax[jj] + d[j];
                if((new_d < d[i]) || ((cm[i] > -1) && (new_d == d[i]) && (cs[cm[j]] < cs[cm[i]]) && (pred_count[i] == 0))){
                    // switch distance/predecessor if the new distance
                    // is strictly shorter, or if (distance is equal, the new
                    // cluster is smaller, and we are not the
                    // predecessor for anyone else)

                    // update cluster sizes
                    if(cm[i] > -1){
                        cs[cm[i]]--;
                        assert(cs[cm[i]] >= 0);
                    }
                    cs[cm[j]]++;

                    // update predecessor assignments and counts
                    if(predecessor[i] > -1){
                        pred_count[predecessor[i]]--;
                        assert(pred_count[predecessor[i]] >= 0);
                    }
                    predecessor[i] = j;
                    pred_count[predecessor[i]]++;

                    // switch to the new cluster
                    d[i] = new_d;
                    cm[i] = cm[j];
                    change = true;
                }
            }
        }
        assert(++iteration < num_nodes*num_nodes*num_nodes); // safety check, regular unweighted BF is actually O(|V|.|E|)
    } while(change);
}


/*
 * Perform one iteration of Lloyd clustering on a distance graph
 *
 *  Parameters
 *      num_nodes       - (IN)  number of nodes (number of rows in A)
 *      Ap[]            - (IN)  CSR row pointer for adjacency matrix A
 *      Aj[]            - (IN)  CSR index array
 *      Ax[]            - (IN)  CSR data array (edge lengths)
 *      num_clusters    - (IN)  number of clusters (seeds)
 *      d[num_nodes]    - (OUT) distance to nearest seed
 *     cm[num_nodes]    - (OUT) cluster index for each node
 *      c[num_clusters] - (IN)  cluster centers
 *
 *  References
 *      Nathan Bell
 *      Algebraic Multigrid for Discrete Differential Forms
 *      PhD thesis (UIUC), August 2008
 *
 */
template<class I, class T>
void lloyd_cluster(const I num_nodes,
                   const I Ap[], const int Ap_size,
                   const I Aj[], const int Aj_size,
                   const T Ax[], const int Ax_size,
                   const I num_clusters,
                         T  d[], const int  d_size,
                         I cm[], const int cm_size,
                         I  c[], const int  c_size)
{
    for(I i = 0; i < num_nodes; i++){
        d[i] = std::numeric_limits<T>::max();
        cm[i] = -1;
    }
    for(I a = 0; a < num_clusters; a++){
        I i = c[a];
        assert(i >= 0 && i < num_nodes);
        d[i] = 0;
        cm[i] = a;
    }

    std::vector<T> old_distances(num_nodes);

    // propagate distances outward
    do{
        std::copy(d, d+num_nodes, old_distances.begin());
        bellman_ford(num_nodes, Ap, Ap_size, Aj, Aj_size, Ax, Ax_size, d, d_size, cm, cm_size);
    } while ( !std::equal( d, d+num_nodes, old_distances.begin() ) );

    //find boundaries
    for(I i = 0; i < num_nodes; i++){
        d[i] = std::numeric_limits<T>::max();
    }
    for(I i = 0; i < num_nodes; i++){
        for(I jj = Ap[i]; jj < Ap[i+1]; jj++){
            I j = Aj[jj];
            if( cm[i] != cm[j] ){
                d[i] = 0;
                break;
            }
        }
    }

    // propagate distances inward
    do{
        std::copy(d, d+num_nodes, old_distances.begin());
        bellman_ford(num_nodes, Ap, Ap_size, Aj, Aj_size, Ax, Ax_size, d, d_size, cm, cm_size);
    } while ( !std::equal( d, d+num_nodes, old_distances.begin() ) );


    // compute new seeds
    for(I i = 0; i < num_nodes; i++){
        const I a = cm[i];

        if (a == -1) //node belongs to no cluster
            continue;

        assert(a >= 0 && a < num_clusters);

        if( d[c[a]] < d[i] )
            c[a] = i;
    }
}


/*
 * Perform one iteration of Lloyd clustering on a distance graph using
 * exact centers
 *
 * This version computes exact cluster centers with Floyd-Warshall and
 * also uses a balanced version of Bellman-Ford to try and find
 * nearly-equal-sized clusters.
 *
 *  Parameters
 *      num_nodes       - (IN)  number of rows in A (number of vertices)
 *      Ap[]            - (IN)  CSR row pointer
 *      Aj[]            - (IN)  CSR index array
 *      Ax[]            - (IN)  CSR data array (edge lengths)
 *      num_clusters    - (IN)  number of clusters = number of seeds
 *      d[num_nodes]    - (OUT) distance to nearest seed
 *     cm[num_nodes]    - (OUT) cluster index for each node
 *      c[num_clusters] - (IN)  cluster centers
 *
 *  References
 *      Nathan Bell
 *      Algebraic Multigrid for Discrete Differential Forms
 *      PhD thesis (UIUC), August 2008
 */
template<class I, class T>
void lloyd_cluster_exact(const I num_nodes,
                         const I Ap[], const int Ap_size,
                         const I Aj[], const int Aj_size,
                         const T Ax[], const int Ax_size,
                         const I num_clusters,
                               T  d[], const int  d_size,
                               I cm[], const int cm_size,
                               I  c[], const int  c_size)
{
    assert(d_size == num_nodes);
    assert(cm_size == num_nodes);
    assert(c_size == num_clusters);

    for(I i = 0; i < num_nodes; i++){
        d[i] = std::numeric_limits<T>::max();
        cm[i] = -1;
    }

    for(I a = 0; a < num_clusters; a++){
        I i = c[a];
        assert(i >= 0 && i < num_nodes);
        d[i] = 0;
        cm[i] = a;
    }

    // assign nodes to the nearest cluster center
    bellman_ford_balanced(num_nodes, num_clusters, Ap, Ap_size, Aj, Aj_size, Ax, Ax_size, d, d_size, cm, cm_size, c, c_size);

    // construct node-cluster incidence arrays
    const I ICp_size = num_nodes;
    I ICp[ICp_size];
    const I ICi_size = num_nodes;
    I ICi[ICi_size];
    const I L_size = num_nodes;
    I L[L_size];
    cluster_node_incidence(num_nodes,
                           num_clusters,
                           cm, cm_size,
                           ICp, ICp_size,
                           ICi, ICi_size,
                           L, L_size);

    // update cluster centers
    for(I a = 0; a < num_clusters; a++){
        c[a] = cluster_center(a, num_nodes, num_clusters, Ap, Ap_size, Aj, Aj_size, Ax, Ax_size, cm, cm_size, ICp, ICp_size, ICi, ICi_size, L, L_size);
        assert(cm[c[a]] == a); // check new center is in cluster
    }
}


/*
 * Propagate (key,value) pairs across a graph in CSR format.
 *
 * Each vertex in the graph looks at all neighboring vertices
 * and selects the (key,value) pair such that the value is
 * greater or equal to every other neighboring value.  If
 * two (key,value) pairs have the same value, the one with
 * the higher index is chosen
 *
 * This method is used within a parallel MIS-k method to
 * propagate the local maximia's information to neighboring
 * vertices at distance K > 1 away.
 *
 */
template<typename IndexType, typename ValueType>
void csr_propagate_max(const IndexType  num_rows,
                       const IndexType  Ap[],
                       const IndexType  Aj[],
                       const IndexType  i_keys[],
                             IndexType  o_keys[],
                       const ValueType  i_vals[],
                             ValueType  o_vals[])
{
    for(IndexType i = 0; i < num_rows; i++){

        IndexType k_max = i_keys[i];
        ValueType v_max = i_vals[i];

        for(IndexType jj = Ap[i]; jj < Ap[i+1]; jj++){
            const IndexType j   = Aj[jj];
            const IndexType k_j = i_keys[j];
            const ValueType v_j = i_vals[j];

            if( k_j == k_max ) continue;
            if( v_j < v_max ) continue;
            if( v_j > v_max || k_j > k_max ){
                k_max = k_j;
                v_max = v_j;
            }
        }

        o_keys[i] = k_max;
        o_vals[i] = v_max;
    }
}

/*
 *  Compute a distance-k maximal independent set for a graph stored
 *  in CSR format using a parallel algorithm.  An MIS-k is a set of
 *  vertices such that all vertices in the MIS-k are separated by a
 *  path of at least K+1 edges and no additional vertex can be added
 *  to the set without destroying this property.  A standard MIS
 *  is therefore a MIS-1.
 *
 *  Parameters
 *      num_rows   - number of rows in A (number of vertices)
 *      Ap[]       - CSR row pointer
 *      Aj[]       - CSR index array
 *      k          - minimum separation between MIS vertices
 *      x[]        - state of each vertex (1 if in the MIS, 0 otherwise)
 *      y[]        - random values used during parallel MIS algorithm
 *      max_iters  - maximum number of iterations to use (default, no limit)
 *
 */
template<class I, class T, class R>
void maximal_independent_set_k_parallel(const I num_rows,
                                        const I Ap[], const int Ap_size,
                                        const I Aj[], const int Aj_size,
                                        const I  k,
                                              T  x[], const int  x_size,
                                        const R  y[], const int  y_size,
                                        const I  max_iters)
{
    std::vector<bool> active(num_rows,true);

    std::vector<I> i_keys(num_rows);
    std::vector<I> o_keys(num_rows);
    std::vector<R> i_vals(num_rows);
    std::vector<R> o_vals(num_rows);

    for(I i = 0; i < num_rows; i++){
        i_keys[i] = i;
        i_vals[i] = y[i];
        x[i] = 0;
    }

    for(I iter = 0; max_iters == -1 || iter < max_iters; iter++){
        for(I i = 0; i < k; i++){
            csr_propagate_max(num_rows, Ap, Aj, &(i_keys[0]), &(o_keys[0]), &(i_vals[0]), &(o_vals[0]));
            std::swap(i_keys, o_keys);
            std::swap(i_vals, o_vals);
        }

        for(I i = 0; i < num_rows; i++){
            if( i_keys[i] == i && active[i]){
                x[i] = 1; // i is a MIS-k node
            }

            i_keys[i] = i;
            i_vals[i] = x[i];
        }

        I rank = 0;
        //while(rank < k && 2*(k - rank) > k){
        //    csr_propagate_max(num_rows, Ap, Aj, &(i_keys[0]), &(o_keys[0]), &(i_vals[0]), &(o_vals[0]));
        //    std::swap(i_keys, o_keys);
        //    std::swap(i_vals, o_vals);
        //    rank++;
        //}

        while(rank < k){
            csr_propagate_max(num_rows, Ap, Aj, &(i_keys[0]), &(o_keys[0]), &(i_vals[0]), &(o_vals[0]));
            std::swap(i_keys, o_keys);
            std::swap(i_vals, o_vals);
            rank++;
        }

        bool work_left = false;

        for(I i = 0; i < num_rows; i++){
            if(i_vals[i] == 1){
                active[i] =  false;
                i_vals[i] = -1;
            } else {
                i_vals[i] = y[i];
                work_left = true;
            }
            i_keys[i] = i;
        }

        if( !work_left )
            return;
    }

}

/*
 *  Compute a breadth first search of a graph in CSR format
 *  beginning at a given seed vertex.
 *
 *  Parameters
 *      num_rows         - number of rows in A (number of vertices)
 *      Ap[]             - CSR row pointer
 *      Aj[]             - CSR index array
 *      order[num_rows]  - records the order in which vertices were searched
 *      level[num_rows]  - records the level set of the searched vertices (i.e. the minimum distance to the seed)
 *
 *  Notes:
 *      The values of the level must be initialized to -1
 *
 */
template <class I>
void breadth_first_search(const I Ap[], const int Ap_size,
                          const I Aj[], const int Aj_size,
                          const I seed,
                                I order[], const int order_size,
                                I level[], const int level_size)
{
    // initialize seed
    order[0]    = seed;
    level[seed] = 0;

    I N = 1;
    I level_begin = 0;
    I level_end   = N;

    I current_level = 1;

    while(level_begin < level_end){
        // for each node of the last level
        for(I ii = level_begin; ii < level_end; ii++){
            const I i = order[ii];

            // add all unmarked neighbors to the queue
            for(I jj = Ap[i]; jj < Ap[i+1]; jj++){
                const I j = Aj[jj];
                if(level[j] == -1){
                    order[N] = j;
                    level[j] = current_level;
                    N++;
                }
            }
        }

        level_begin = level_end;
        level_end   = N;
        current_level++;
    }

}


/*
 *  Compute the connected components of a graph stored in CSR format.
 *
 *  Vertices belonging to each component are marked with a unique integer
 *  in the range [0,K), where K is the number of components.
 *
 *  Parameters
 *      num_rows             - number of rows in A (number of vertices)
 *      Ap[]                 - CSR row pointer
 *      Aj[]                 - CSR index array
 *      components[num_rows] - component labels
 *
 */
template <class I>
I connected_components(const I num_nodes,
                       const I Ap[], const int Ap_size,
                       const I Aj[], const int Aj_size,
                             I components[], const int components_size)
{
    std::fill(components, components + num_nodes, -1);
    std::stack<I> DFS;
    I component = 0;

    for(I i = 0; i < num_nodes; i++)
    {
        if(components[i] == -1)
        {
            DFS.push(i);
            components[i] = component;

            while (!DFS.empty())
            {
                I top = DFS.top();
                DFS.pop();

                for(I jj = Ap[top]; jj < Ap[top + 1]; jj++){
                    const I j = Aj[jj];
                    if(components[j] == -1){
                        DFS.push(j);
                        components[j] = component;
                    }
                }
            }

            component++;
        }
    }

    return component;
}

#endif
