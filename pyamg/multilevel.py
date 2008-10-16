"""Generic AMG solver"""

__docformat__ = "restructuredtext en"

import scipy
import numpy
from numpy import ones, zeros, zeros_like, array, asarray, empty, asanyarray, ravel
from scipy.sparse import csc_matrix

#from pyamg import relaxation
from pyamg.relaxation import *
from utils import symmetric_rescaling, diag_sparse, norm

__all__ = ['multilevel_solver', 'coarse_grid_solver']


class multilevel_solver:
    """Stores multigrid hierarchy and implements the multigrid cycle

    The class constructs the cycling process and points to the methods for
    coarse grid solves.  A multilevel_solver object is typically returned from
    a particular AMG method (see ruge_stuben_solver or
    smoothed_aggregation_solver for example).  A call to
    multilevel_solver.solve() is a typical access point.  The class also
    defines methods for constructing operator, cycle, and grid complexities. 

    Attributes
    ----------
    ccx : float
        Tracks the total operations (O(nnz) per smoothing sweep)
    first_pass : {True,False}
        Indicates whether the solver is in its first cycle
    levels : level array
        Array of level objects that contain A, R, and P.
    preprocess : function pointer
        Optional function to manipulate x and b at the start of the cycle
    postprocess : function pointer
        Optional function to manipulate x and b at the end of the cycle
    coarse_solver : string
        String passed to coarse_grid_solver indicating the solve type

    Methods
    -------
    cycle_complexity(lvl=-1)
        Returns the cycle complexity or updates for a given level (lvl>=0)
    operator_complexity()
        Returns the operator complexity
    grid_complexity()
        Returns the operator complexity
    solve(b, x0=None, tol=1e-5, maxiter=100, callback=None, return_residuals=False, cycle='V')
        The main multigrid solve call.
    """

    class level:
        """Stores one level of the multigrid hierarchy

        All level objects will have an 'A' attribute referencing the matrix
        of that level.  All levels, except for the coarsest level, will 
        also have 'P' and 'R' attributes referencing the prolongation and 
        restriction operators that act between each level and the next 
        coarser level.

        Attributes
        ----------
        A : csr_matrix
            Problem matrix for `A`x=b
        R : csr_matrix
            Restriction matrix between levels (often `R` = `P`.T)
        P : csr_matrix
            Prolongation or Interpolation matrix.

        Notes
        -----
        The functionality of this class is a struct
        """
        pass

    def __init__(self, levels, preprocess=None, postprocess=None, coarse_solver='pinv2'):
        """
        Class constructor responsible for initializing the cycle and ensuring
        the list of levels is complete.

        Parameters
        ----------
        ccx : float
            Tracks the total operations (O(nnz) per smoothing sweep)
        first_pass : {True,False}
            Indicates whether the solver is in its first cycle
        levels : level array
            Array of level objects that contain A, R, and P.
        preprocess : function pointer
            Optional function to manipulate x and b at the start of the cycle
        postprocess : function pointer
            Optional function to manipulate x and b at the end of the cycle
        coarse_solver : string
            String passed to coarse_grid_solver indicating the solve type
    
        Notes
        -----
        R is set to P.T unless previously set.

        """
        self.ccx = 0
        self.first_pass=True
        self.levels = levels
        
        self.preprocess  = preprocess
        self.postprocess = postprocess

        self.coarse_solver = coarse_grid_solver(coarse_solver)

        for level in levels[:-1]:
            if not hasattr(level, 'R'):
                level.R = level.P.T

    def __repr__(self):
        """
        Prints statistics about the fixed multigrid heirarchy.  Does not print
        information from solving.
        """
        output = 'multilevel_solver\n'
        output += 'Number of Levels:     %d\n'   % len(self.levels)
        output += 'Operator Complexity: %6.3f\n' % self.operator_complexity()
        output += 'Grid Complexity:     %6.3f\n' % self.grid_complexity()

        total_nnz =  sum([level.A.nnz for level in self.levels])

        output += '  level   unknowns     nonzeros\n'
        for n,level in enumerate(self.levels):
            A = level.A
            output += '   %2d   %10d   %10d [%5.2f%%]\n' % (n,A.shape[1],A.nnz,(100*float(A.nnz)/float(total_nnz)))

        return output

    def cycle_complexity(self,lvl=-1):
        """Number of nonzeros in the matrix on all levels of a cycle/ 
        number of nonzeros in the matrix on the finest level.  This is 
        approximately 2*operator_complexity for a V-Cycle."""
        if(lvl>-1 and self.first_pass==True):
            self.ccx += self.levels[lvl].A.nnz
        else:
            return self.ccx/float(self.levels[0].A.nnz)

    def operator_complexity(self):
        """Number of nonzeros in the matrix on all levels / 
        number of nonzeros in the matrix on the finest level"""
        return sum([level.A.nnz for level in self.levels])/float(self.levels[0].A.nnz)

    def grid_complexity(self):
        """Number of unknowns on all levels / 
        number of unknowns on the finest level"""
        return sum([level.A.shape[0] for level in self.levels])/float(self.levels[0].A.shape[0])

    def psolve(self, b):
        return self.solve(b,maxiter=1)

    def solve(self, b, x0=None, tol=1e-5, maxiter=100, callback=None, return_residuals=False, cycle='V'):
        """
        Main solution call to execute multigrid cycling.

        Parameters
        ----------
        b : numpy_array
            Right hand side.
        x0 : numpy_array
            Initial guess.
        tol : float
            Stopping criteria for the relative residual r[k]/r[0].
        maxiter : int
            Stopping criteria for the maximum number of allowable iterations.
        callback : function pointer
            Function processed after each cycle (iteration).
        return_residuals : {True,False}
            Flag to return a vector of residuals.
        cycle : {'V','W','F'}
            Character to indicate the type of cycling to perform in each
            iteration.

        Returns
        -------
        x : numpy_array
            Approximate solution to Ax=b
        residuals : numpy_array
            Optional return based on return_residuals.  Vector of residuals
            after each cycle.

        See Also
        --------
        __solve : private function implementing the recurssive calls.

        Examples
        --------
        >>>> from numpy import ones
        >>>> from scipy.sparse import spdiags
        >>>> from pyamg.classical import ruge_stuben_solver
        >>>> n=100
        >>>> e = ones((n,1)).ravel()
        >>>> data = [ -1*e, 2*e, -1*e ]
        >>>> A = spdiags(data,[-1,0,1],n,n)
        >>>> b = A*ones(A.shape[0])
        >>>> ml = ruge_stuben_solver(A, max_coarse=10)
        >>>> x, resvec = ml.solve(b, tol=1e-14, return_residuals=True)
        """

        if x0 is None:
            x = zeros_like(b)
        else:
            x = array(x0) #copy

        if self.preprocess is not None:
            x,b = self.preprocess(x,b)

        A = self.levels[0].A
        residuals = [ norm(b-A*x) ]

        self.first_pass=True

        while len(residuals) <= maxiter and residuals[-1]/residuals[0] > tol:
            if len(self.levels) == 1:
                # hierarchy has only 1 level
                x = self.coarse_solver(A,b)
            else:
                self.__solve(0,x,b,cycle)

            residuals.append( norm(b-A*x) )

            self.first_pass=False

            if callback is not None:
                callback(x)

        if self.postprocess is not None:
            x = self.postprocess(x)

        if return_residuals:
            return x,residuals
        else:
            return x

    def __solve(self,lvl,x,b,cycle='V'):
        """
        Parameters
        ----------
        lvl : int
            Solve problem on level `lvl`
        x : numpy array
            Initial guess `x` and return correction
        b : numpy array
            Right-hand side for Ax=b
        cycle : {'V','W','F',int}
            Recursively called cycling function.  The 
            Defines the cycling used:
            cycle = 'F', F-cycle
            cycle = 'V', V-cycle
            cycle = 'W', W-cycle, default

        Notes
        -----
        The cycle complexity ccx is update by nnz for each hit of the smoother.
        nu1 and nu2 pre/post smoothing sweeps will not impact the cycle
        complexity.  Moreover, the coarse level solve also assumes nnz time.
        """
        if str(cycle).upper() not in ['V','W','F']:
            raise TypeError('Unrecognized cycle type')

        A = self.levels[lvl].A

        self.levels[lvl].presmoother(A,x,b)
        self.cycle_complexity(lvl)

        residual = b - A*x

        coarse_b = self.levels[lvl].R * residual
        coarse_x = zeros_like(coarse_b)

        if lvl == len(self.levels) - 2:
            coarse_x[:] = self.coarse_solver(self.levels[-1].A, coarse_b)
            self.cycle_complexity(lvl)
        else:
            if(cycle.upper()=='F'):
                self.__solve(lvl + 1, coarse_x, coarse_b,cycle)
                self.__solve(lvl + 1, coarse_x, coarse_b,cycle='V')
            else:
                self.__solve(lvl + 1, coarse_x, coarse_b,cycle)
                if(cycle.upper()=='W'):
                    self.__solve(lvl + 1, coarse_x, coarse_b,cycle)

        x += self.levels[lvl].P * coarse_x   #coarse grid correction

        self.levels[lvl].postsmoother(A,x,b)
        self.cycle_complexity(lvl)

#    def presmooth(self,A,x,b):
#        def unpack_arg(v):
#            if isinstance(v,tuple):
#                return v[0],v[1]
#            else:
#                return v,{}
#
#        fn, kwargs = unpack_arg(self.presmoother)
#        if fn == 'gauss_seidel':
#            gauss_seidel(A, x, b, **kwargs)
#        elif fn == 'kaczmarz_gauss_seidel':
#            kaczmarz_gauss_seidel(A, x, b, **kwargs)
#        else:
#            raise TypeError('Unrecognized presmoother')
#        #fn = relaxation.dispatch(self.presmoother)
#        #fn(A,x,b)
#
#    def postsmooth(self,A,x,b):
#        def unpack_arg(v):
#            if isinstance(v,tuple):
#                return v[0],v[1]
#            else:
#                return v,{}
#        
#        fn, kwargs = unpack_arg(self.presmoother)
#        if fn == 'gauss_seidel':
#            gauss_seidel(A, x, b, **kwargs)
#        elif fn == 'kaczmarz_gauss_seidel':
#            kaczmarz_gauss_seidel(A, x, b, **kwargs)
#        else:
#            raise TypeError('Unrecognized postsmoother')
#        #fn = relaxation.dispatch(self.postsmoother)
#        #fn(A,x,b)


#TODO support (solver,opts) format also
def coarse_grid_solver(solver):
    """Return a coarse grid solver suitable for multilevel_solver
    
    Parameters
    ----------
    solver: string
        Sparse direct methods:
            splu         : sparse LU solver
        Sparse iterative methods:
            the name of any method in scipy.sparse.linalg.isolve (e.g. 'cg')
        Dense methods:
            pinv     : pseudoinverse (QR)
            pinv2    : pseudoinverse (SVD)
            lu       : LU factorization 
            cholesky : Cholesky factorization

    TODO add relaxation methods

    Examples
    --------

    TODO
        
    """
    
    if solver in ['pinv', 'pinv2']:
        def solve(self,A,b):
            if not hasattr(self, 'P'):
                self.P = getattr(scipy.linalg, solver)( A.todense() )
            return numpy.dot(self.P,b)
    
    elif solver == 'lu':
        def solve(self,A,b):
            if not hasattr(self, 'LU'):
                self.LU = scipy.linalg.lu_factor( A.todense() )
            return scipy.linalg.lu_solve(self.LU, b)

    elif solver == 'cholesky':
        def solve(self,A,b):
            if not hasattr(self, 'L'):
                self.L = scipy.linalg.cho_factor( A.todense() )
            return scipy.linalg.cho_solve(self.L, b)
    
    elif solver == 'splu':
        def solve(self,A,b):
            if not hasattr(self, 'LU'):
                self.LU = scipy.sparse.linalg.splu( csc_matrix(A) )
            return self.LU.solve( ravel(b) )
    
    elif solver in ['bicg','bicgstab','cg','cgs','gmres','qmr','minres']:
        fn = getattr(scipy.sparse.linalg.isolve, solver)
        def solve(self,A,b):
            return fn(A, b, tol=1e-12)[0]

    elif solver is None:         
        # Identity
        def solve(self,A,b):
            return 0*b
    else:
        raise ValueError,('unknown solver: %s' % fn)
       
    #TODO handle case A.nnz == 0

    def wrapped_solve(self,A,b):
        # make sure x is same dimensions and type as b
        b = asanyarray(b)
        x = solve(self,A,b)
        if isinstance(b,numpy.ndarray):
            x = asarray(x)
        elif isinstance(b,numpy.matrix):
            x = asmatrix(x)
        else:
            raise ValueError('unrecognized type')
        return x.reshape(b.shape)

    class generic_solver:
        __call__ = wrapped_solve

    return generic_solver()
    

