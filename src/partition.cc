/*
 * =====================================================================================
 *
 *       Filename:  partition.cc
 *
 *    Description:  Partition the graph according to the eigenvectors
 *        Created:  06/17/2016 14:15:15
 *
 *         Author:  Ken Hu, xnchnhu@gmail.com
 *
 * =====================================================================================
 */

#include <iostream>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>
#include <unordered_map>

#include <boost/timer.hpp>

#include "partition.h"
#include "lanczos.h"
#include "tqli.h"
//#include "vt_user.h"

#define Sign(a) (a >= 0.0 ? 1:0)

using namespace std;

typedef std::vector<double> Vector;
typedef std::vector<Vector> DenseMatrix;

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  Constructor
 *  Description:  Partition the graph into multiple subgraphs by only calculating the corresponding laplacian eigenvectors
 * =====================================================================================
 */

Partition::Partition(const Graph& g, const int& subgraphs, bool GramSchmidt) {

    //VT_TRACER("PARTITION");
    boost::timer timer_partition;
    int size = g.size();
    int num_of_eigenvec = log2(subgraphs);

    // Construct tridiagonal matrix using Lanczos algorithm

    boost::timer timer_lanczos;
    Lanczos<Vector, double> lanczos(g, num_of_eigenvec, GramSchmidt);
    double t_lan = timer_lanczos.elapsed();
    cout << "Lanczos timer - Lanczos takes " << t_lan << "s" << endl;
    times.push_back(t_lan);
    laplacian_eigenvalues_ = lanczos.alpha;
    Vector beta = lanczos.beta;

#ifdef Debug
    cout << endl;
    cout << "triangular matrix: " << endl;
    lanczos.print_tri_mat();
#endif

    // Define an identity matrix as the input for TQLI algorithm
    DenseMatrix tri_eigen_vecs;

    // Calculate the eigenvalues and eigenvectors of the tridiagonal matrix
    boost::timer timer_tqli;
    tqli(laplacian_eigenvalues_, beta, tri_eigen_vecs);
    double t_tqli = timer_tqli.elapsed();
    cout << "TQLI timer - TQLI takes " << t_tqli << "s" << endl;
    times.push_back(t_tqli);

    // Find the index of the nth smallest eigenvalue (fiedler vector) of the eigenvalues vector "alpha"
    int vector_index = 0;

    int m = laplacian_eigenvalues_.size();
    unordered_multimap<double, int> hashmap;
    for (int i = 0; i < m; i++) {
        hashmap.insert({laplacian_eigenvalues_[i], i});
    }

    Vector auxiliary_vec = laplacian_eigenvalues_;
    sort(auxiliary_vec.begin(), auxiliary_vec.end());
    int fiedler_index = 1;
    //int fiedler_index = laplacian_eigenvalues_.size() - 1;
    for (int i = 0; i < num_of_eigenvec; i++) {
        auto it = hashmap.find(auxiliary_vec[fiedler_index]);
        while (abs(it->first) < 1e-2) {
            fiedler_index++;
            it = hashmap.find(auxiliary_vec[fiedler_index]);
        }
        fiedler_index++;
        //fiedler_index--;
        vector_index = it->second;
        ritz_values.push_back(it->first);
        //cout << "eigenvalue used: " << it->first << ", Vector_Index: " << vector_index <<endl;
        hashmap.erase(it); // Deal with identical eigenvalues
        laplacian_eigen_mat_.push_back(getOneLapEigenVec(lanczos.lanczos_vecs, tri_eigen_vecs, vector_index));
    }

    for (int vertex = 0; vertex < size; vertex++) {
        int colour = 0;
        for (int row = 0; row < num_of_eigenvec; row++) {
            colour += pow(2, row) * Sign(laplacian_eigen_mat_[row][vertex]);
        }
        g.setColour(vertex, colour);
    }
    double t_par = timer_partition.elapsed();
    cout << "Partition timer - partition takes " << t_par << "s" << endl;
    times.push_back(t_par);
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  utilities
 *  Description:  Print vector, matrix for debug
 * =====================================================================================
 */

void Partition::printLapEigenMat() {
    int row_size = laplacian_eigen_mat_.size();
    for (int row = 0; row < row_size; row++) {
        int col_size = laplacian_eigen_mat_[row].size();
        for (int col = 0; col < col_size; col++) {
            cout << laplacian_eigen_mat_[row][col] << " ";
        }
        cout << endl;
    }
}

void Partition::printLapEigenvalues() {
    cout << "Laplacian Eigenvalues: ";
    for (const double& x:laplacian_eigenvalues_) {
        cout << x <<  " ";
    }
    cout << endl;
}

void Partition::outputLapEigenvalues() {

    string filename("eigenvalues_");
    filename += to_string(laplacian_eigenvalues_.size());
    filename += ".dat";

    ofstream Output(filename);

    for (unsigned int i = 0; i < laplacian_eigenvalues_.size(); i++)	{
        Output << i << " " << laplacian_eigenvalues_[i] << endl;
    }

    Output.close();
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  getOneLapEigenVec
 *  Description:  Return the fiedler vector using lanczos and tqli functions
 * =====================================================================================
 */

Vector Partition::getOneLapEigenVec(DenseMatrix& lanczos_vecs, DenseMatrix& tri_eigen_vecs, const int& vector_index) {
    //VT_TRACER("EigenVec");
    // Find the eigenvector from the vector matrix of the Tridiagonal eigenvector matrix (each column represents a vector)
    int size = tri_eigen_vecs.size(); // Size of vertices/vectors
    Vector tri_eigen_vec(size, 0);
    for (int row = 0; row < size; row++) {
        for (int col = 0; col < size; col++) {
            if (col == vector_index) {
                tri_eigen_vec[row] = tri_eigen_vecs[row][col];
            }
        }
    }
    // Calculate the corresponding Laplacian vector by Lanczos vectors(each row represents a vector, need transposing)
    // lanczos vector - m * n, tri_eigen_vecs - m * m, Ritz - m * n, Ritz_vector - n
    int col_size = lanczos_vecs[0].size();
    int row_size = lanczos_vecs.size();
    Vector laplacian_vector(col_size, 0);
    for (int col = 0; col < col_size; col++) {
        for (int row = 0; row < row_size; row++) {
            laplacian_vector[col] += lanczos_vecs[row][col] * tri_eigen_vec[row];
        }
    }
    return laplacian_vector;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  getLapEigenMat
 *  Description:  Get all the eigenvectors of the original matrix.
 * =====================================================================================
 */

void Partition::getLapEigenMat(const Graph& g, bool GramSchmidt) {

    int size = g.size();
    Vector vinit(size, 0);
    for(int i = 0; i < size; i++)  laplacian_eigen_mat_[i] = vinit;

    // Construct tridiagonal matrix using Lanczos algorithm
    Lanczos<Vector, double> lanczos(g, size, GramSchmidt);
    laplacian_eigenvalues_ = lanczos.alpha;
    Vector beta = lanczos.beta;

#ifdef Debug
    cout << endl;
    cout << "triangular matrix: " << endl;
    lanczos.print_tri_mat();
#endif

    // Define an identity matrix as the input for TQLI algorithm
    DenseMatrix tri_eigen_vecs;

    // Calculate the eigenvalues and eigenvectors of the tridiagonal matrix
    tqli(laplacian_eigenvalues_, beta, tri_eigen_vecs);

    // Calculate all the eigenvectors of original Laplacian matrix using
    // the eigenvectors of the tridiagonal matrix computed by TQLI
    for (int k = 0; k < size; k++) {
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < size; j++) {
                laplacian_eigen_mat_[k][i] += lanczos.lanczos_vecs[j][i] * tri_eigen_vecs[j][k];
            }
        }
    }
}
