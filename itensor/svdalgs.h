//
// Distributed under the ITensor Library License, Version 1.1.
//    (See accompanying LICENSE file.)
//
#ifndef __ITENSOR_SVDALGS_H
#define __ITENSOR_SVDALGS_H
#include "iqcombiner.h"
#include "localop.h"
#include "spectrum.h"


namespace itensor {


//
// Singular value decomposition (SVD)
//
// Factors a tensor AA such that AA=U*D*V
// with D diagonal, real, and non-negative.
//
template<class Tensor>
Spectrum 
svd(Tensor AA, Tensor& U, Tensor& D, Tensor& V, 
    const Args& args = Global::args());

//
// Density Matrix Decomposition
// 
// Factors a tensor AA such that AA = A*B.
// Result is equivalent to SVD such that AA = U*D*V where if
// dir==Fromleft, A=U and B=(D*V) or, if dir==Fromright, A=(U*D) and B=V.
// Implementation is faster than SVD, though, and allows the
// noise term to be used.
//
// To determine which indices end up on which factors (i.e. on A versus B),
// the method examines the initial indices of A and B.
// If a given index is present on, say, A, then it will on A 
// upon return (although the elements of A will be overwritten and other indices
// may be added to it). Any indices not initially present on A or B 
// will end up on B if dir==Fromleft or on A if dir==Fromright.
//
template<class Tensor>
Spectrum 
denmatDecomp(const Tensor& AA, Tensor& A, Tensor& B, Direction dir, 
             const Args& args = Global::args())
    {
    return denmatDecomp(AA,A,B,dir,LocalOp<Tensor>::Null(),args);
    }

//Density matrix decomp with LocalOpT object supporting the noise term
//The LocalOpT argument PH has to provide the deltaRho method
//to enable the noise term feature (see localop.h for example)
template<class Tensor, class LocalOpT>
Spectrum 
denmatDecomp(const Tensor& AA, Tensor& A, Tensor& B, Direction dir, 
             const LocalOpT& PH,
             Args args = Global::args());



//
// Hermitian eigenvalue decomposition / diagonalization
//
// Assumes input is a Hermitian tensor with indices
// i,j,k,.... and i',j',k',...
// (tensor must be conjugate symmetric under
//  exchange primed and unprimed indices)
// Result is unitary tensor U and diagonal sparse tensor D
// such that M == dag(U)*D*prime(U)
//
template<class Tensor>
Spectrum 
diagHermitian(const Tensor& M, Tensor& U, Tensor& D, 
              Args args = Global::args());




//
// Orthogonal decomposition
//
// Given a tensor T, decomposes it into two tensors A and B
// such that T=A*B. If dir==Fromleft, A is guaranteed to be
// real and orthogonal, similar for B if dir==Fromright.
//
template<class Tensor>
Spectrum 
orthoDecomp(Tensor T, Tensor& A, Tensor& B, 
            Direction dir, 
            const Args& args = Global::args());


//
// Inverse Canonical SVD
//
// Factors a tensor AA such that AA=L*V*R
// where V is the inverse of the diagonal tensor
// appearing in the SVD
//

template<class Tensor>
Spectrum 
csvd(const Tensor& AA, Tensor& L, Tensor& V, Tensor& R, 
     const Args& args = Global::args());


//
//
// Eigen decomposition
//
// Computes eigenvalues V and eigenvectors D of an arbitrary tensor T.
// T must be "square-matrix-like" in one of the following two ways: 
//
// (1) T has only indices I,J,K,... and indices I',J',K',...
//     If V is default-constructed upon calling eigDecomp this case is assumed.
//
// (2) V has "column" indices I,J,K,... and T has these indices as well.
//     T also has "row" indices L,M,N,... such that grouping (I,J,K,...) and
//     (L,M,N,...) transforms T into a square matrix.
//
// D is a diagonal rank 2 tensor (matrix) containing the eigenvalues.
// On return, V has the "column" indices of T and a new index shared with D
// (the index labeled "C" below).
//
// The result is such that V and D give:
//      __         __               __
// K-<-|  |-<-I-<-|  |         K-<-|~ |     
//     |T |       |V |-<-C ==      |V |-<-C'-<-(D)-<-C
// L-<-|__|-<-J-<-|__|         L-<-|__|    
//
//       ~
// (here V is identical to V upon replacing K->I, L->J,
//  and for case (1) is the same as prime(V))
//
template<class Tensor>
void 
eigDecomp(const Tensor& T, Tensor& V, Tensor& D,
          const Args& args = Global::args());


///////////////////////////
//
// Implementation (non-template parts in svdalgs.cc)
//
//////////////////////////


Spectrum 
svdRank2(ITensor A, const Index& ui, const Index& vi,
         ITensor& U, ITensor& D, ITensor& V,
         const Args& args = Global::args());

Spectrum 
svdRank2(IQTensor A, const IQIndex& uI, const IQIndex& vI,
         IQTensor& U, IQTensor& D, IQTensor& V,
         const Args& args = Global::args());

template<class Tensor>
Spectrum 
svd(Tensor AA, Tensor& U, Tensor& D, Tensor& V, 
    const Args& args)
    {
    using IndexT = typename Tensor::IndexT;
    using CombinerT = typename Tensor::CombinerT;

    const Real noise = args.getReal("Noise",0.);
    const bool useOrigM = args.getBool("UseOrigM",false);
    const Args* args_ = &args;
    
    if(isZero(AA,Args("Fast"))) 
        throw ResultIsZero("svd: AA is zero");

    if(noise > 0)
        Error("Noise term not implemented for svd");

    //Combiners which transform AA
    //into a rank 2 tensor
    CombinerT Ucomb, Vcomb;

    //Divide up indices based on U
    //If U is null, use V instead
    const Tensor &L = (U ? U : V);
    CombinerT &Lcomb = (U ? Ucomb : Vcomb),
              &Rcomb = (U ? Vcomb : Ucomb);
    for(const IndexT& I : AA.indices())
        { 
        if(hasindex(L,I))
            Lcomb.addleft(I);
        else
            Rcomb.addleft(I);
        }

    AA = Ucomb * AA * Vcomb;

    Args newArgs(args);
    if(useOrigM)
        {
        //Try to determine current m,
        //then set minm_ and maxm_ to this.
        newArgs.add("Cutoff",-1);
        int minm = 1,
            maxm = MAX_M;
        if(D.r() == 0)
            {
            IndexT mid = commonIndex(U,V,Link);
            if(mid) minm = maxm = mid.m();
            else    minm = maxm = 1;
            }
        else
            {
            minm = maxm = D.indices().front().m();
            }
        newArgs.add("Minm",minm);
        newArgs.add("Maxm",maxm);
        args_ = &newArgs;
        }

    Spectrum spec = 
    svdRank2(AA,Ucomb.right(),Vcomb.right(),U,D,V,*args_);

    U = dag(Ucomb) * U;
    V = V * dag(Vcomb);

    return spec;

    } //svd

template<class Tensor>
Spectrum 
csvd(const Tensor& AA, Tensor& L, Tensor& V, Tensor& R, 
     const Args& args)
    {
    Tensor UU(L),VV(R);
    Tensor D(V);
    Spectrum spec = svd(AA,UU,D,VV,args);

    L = UU*D;
    R = D*VV;

    V = dag(D);
    V.pseudoInvert(0);
    return spec;
    }

template<class Tensor, class LocalOpT>
Spectrum 
denmatDecomp(const Tensor& AA, Tensor& A, Tensor& B, 
             Direction dir, 
             const LocalOpT& PH,
             Args args)
    {
    using IndexT = typename Tensor::IndexT;
    using CombinerT = typename Tensor::CombinerT;

    const Real noise = args.getReal("Noise",0.);

    if(isZero(AA,Args("Fast"))) 
        {
        throw ResultIsZero("denmatDecomp: AA is zero");
        }

    IndexT mid = commonIndex(A,B,Link);

    //If dir==None, put the O.C. on the side
    //that keeps mid's arrow the same
    if(dir == None)
        {
        dir = (mid.dir() == Out ? Fromright : Fromleft);
        }

    Tensor& to_orth = (dir==Fromleft ? A : B);
    Tensor& newoc   = (dir==Fromleft ? B : A);
    
    CombinerT comb;

    const IndexSet<IndexT>& activeInds = (to_orth ? to_orth : AA).indices();

    for(const IndexT& I : activeInds)
        { 
        if(!hasindex(newoc,I))
            comb.addleft(I);
        }

    //Apply combiner
    comb.init(mid ? mid.rawname() : "mid");

    Tensor AAc; 
    comb.product(AA,AAc);

    //Form density matrix
    Tensor AAcc = dag(AAc); 
    AAcc.prime(comb.right()); 

    Tensor rho = AAc*AAcc; 

    //Add noise term if requested
    if(noise > 0 && !PH.isNull())
        {
        rho += noise*PH.deltaRho(AA,comb,dir);
        rho *= 1./trace(realPart(rho));
        }

    if(args.getBool("UseOrigM",false))
        {
        args.add("Cutoff",-1);
        args.add("Minm",mid.m());
        args.add("Maxm",mid.m());
        }

    if(args.getBool("TraceReIm",false))
        {
        rho = realPart(rho);
        }

    Tensor U;
    Tensor D;
    args.add("Truncate",true);
    Spectrum spec = diag_hermitian(rho,U,D,args);

    comb.dag();
    comb.product(dag(U),to_orth);
    newoc = U * AAc;

    return spec;

    } //denmatDecomp


Spectrum 
diag_hermitian(ITensor rho, ITensor& U, ITensor& D,
               const Args& args = Global::args());

Spectrum 
diag_hermitian(IQTensor rho, IQTensor& U, IQTensor& D,
               const Args& args = Global::args());


template<class Tensor>
Spectrum 
diagHermitian(const Tensor& M, Tensor& U, Tensor& D,
              Args args)
    {
    using IndexT = typename Tensor::IndexT;
    using CombinerT = typename Tensor::CombinerT;

    if(isZero(M,Args("Fast"))) 
        throw ResultIsZero("denmatDecomp: M is zero");

    CombinerT comb;
    for(const IndexT& I : M.indices())
        { 
        if(I.primeLevel() == 0)
            {
            comb.addleft(I);
            }
        }

    //Apply combiner
    comb.init("d");

    Tensor Mc; 
    comb.product(M,Mc);

    CombinerT combP(comb);
    combP.prime();
    combP.dag();

    try {
        Mc = combP * Mc;
        }
    catch(const ITError& e)
        {
        println("Diagonalize expects opposite arrow directions for primed and unprimed indices.");
        throw e;
        }

    Spectrum spec = diag_hermitian(Mc,U,D,args);

    U = comb * U;

    return spec;

    } //diagHermitian


template<class Tensor>
Spectrum 
orthoDecomp(Tensor T, Tensor& A, Tensor& B, 
            Direction dir, 
            const Args& args)
    {
    using IndexT = typename Tensor::IndexT;
    using CombinerT = typename Tensor::CombinerT;

    if(isZero(T,Args("Fast"))) 
        throw ResultIsZero("orthoDecomp: T is zero");

    const
    bool usedenmat = false;

    Spectrum spec;

    if(usedenmat)
        {
        spec = denmatDecomp(T,A,B,dir,args + Args("TraceReIm",true,"Noise",0));
        }
    else //use svd
        {
        //Combiners which transform T
        //into a rank 2 tensor
        CombinerT Acomb, Bcomb;

        const
        IndexT reim = IQIndex("ReIm",Index("reim",2),QN());

        //Divide up indices based on U
        //If U is null, use V instead
        const Tensor &L = (A ? A : B);
        CombinerT &Lcomb = (A ? Acomb : Bcomb),
                  &Rcomb = (A ? Bcomb : Acomb);
        for(const IndexT& I : T.indices())
            { 
            if(hasindex(L,I))
                Lcomb.addleft(I);
            else
                Rcomb.addleft(I);
            }

        if(dir == Fromleft)
            Rcomb.addleft(reim);
        else
            Lcomb.addleft(reim);

        T = realPart(T)*reim(1) + imagPart(T)*reim(2);

        T = Acomb * T * Bcomb;


        Tensor D;
        spec = svdRank2(T,Acomb.right(),Bcomb.right(),A,D,B,args);

        A = dag(Acomb) * A;
        B = B * dag(Bcomb);

        if(dir==Fromleft) 
            {
            B *= D;
            B = B*dag(reim)(1) + Complex_i*B*dag(reim)(2);
            }
        else              
            {
            A *= D;
            A = A*dag(reim)(1) + Complex_i*A*dag(reim)(2);
            }
        }

    return spec;

    } //orthoDecomp

void 
eig_decomp(ITensor T, const Index& L, const Index& R, ITensor& V, ITensor& D,
           const Args& args = Global::args());

void 
eig_decomp(IQTensor T, const IQIndex& L, const IQIndex& R, IQTensor& V, IQTensor& D,
           const Args& args = Global::args());

template<class Tensor>
void 
eigDecomp(const Tensor& T, Tensor& V, Tensor& D,
          const Args& args)
    {
    using IndexT = typename Tensor::IndexT;
    using CombinerT = typename Tensor::CombinerT;

    if(isZero(T,Args("Fast"))) 
        throw ResultIsZero("eigDecomp: T is zero");

    CombinerT ccomb, //common or column indices
              rcomb; //remaining or row indices
    if(V.r() != 0)
        {
        //Use indices of V as "column" indices
        for(const IndexT& I : T.indices())
            { 
            if(hasindex(V,I))
                ccomb.addleft(I);
            else
                rcomb.addleft(I);
            }
        }
    else
        {
        //No hint from V, 
        //separate indices by primelevel
        for(const IndexT& I : T.indices())
            { 
            if(I.primeLevel() == 0)
                ccomb.addleft(I);
            else
                rcomb.addleft(I);
            }
        }

    Tensor Tc = rcomb * T * ccomb; 

    if(rcomb.right().m() != ccomb.right().m())
        {
        Error("Tensor not square-matrix-like in eigDecomp");
        }

    eig_decomp(Tc,rcomb.right(),ccomb.right(),V,D,args);

    V = V * ccomb;
    }

} //namespace itensor


#endif
