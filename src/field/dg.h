#ifndef __DG_H
#define __DG_H

#define forEachLgl(i_,j_,k_) 							\
			for(Int i_ = 0;i_ < NPX;i_++)				\
				for(Int j_ = 0;j_ < NPY;j_++)			\
					for(Int k_ = 0;k_ < NPZ;k_++)
#define forEachLglXY(i_,j_) 							\
			for(Int i_ = 0;i_ < NPX;i_++)				\
				for(Int j_ = 0;j_ < NPY;j_++)
#define forEachLglXZ(i_,k_) 							\
			for(Int i_ = 0;i_ < NPX;i_++)				\
				for(Int k_ = 0;k_ < NPZ;k_++)
#define forEachLglYZ(j_,k_) 							\
			for(Int j_ = 0;j_ < NPY;j_++)				\
				for(Int k_ = 0;k_ < NPZ;k_++)					
#define forEachLglX(i_)									\
			for(Int i_ = 0;i_ < NPX;i_++)
#define forEachLglY(j_)									\
			for(Int j_ = 0;j_ < NPY;j_++)
#define forEachLglZ(k_)									\
			for(Int k_ = 0;k_ < NPZ;k_++)
									
#define INDEX4(c_,i_,j_,k_) \
	((c_) * NPX * NPY * NPZ + (i_) * NPY * NPZ + (j_) * NPZ + (k_))
	
#define INDEX3(i_,j_,k_) \
	((i_) * NPY * NPZ + (j_) * NPZ + (k_))
	
#define DPSI(i,j,k)															\
	Scalar dpsi_j_0 = dpsi[0][ii][i] *   psi[1][jj][j] *   psi[2][kk][k];	\
	Scalar dpsi_j_1 =  psi[0][ii][i] *  dpsi[1][jj][j] *   psi[2][kk][k];	\
	Scalar dpsi_j_2 =  psi[0][ii][i] *   psi[1][jj][j] *  dpsi[2][kk][k];	\
	Vector dpsi_j = Vector(dpsi_j_0,dpsi_j_1,dpsi_j_2);
			
namespace DG {
	extern Scalar **psi[3];
	extern Scalar **dpsi[3];
	extern Scalar *xgl[3];
	extern Scalar *wgl[3];
	extern TensorCellField Jinv;
	extern Int NPX,NPY,NPZ;
	
	void legendre(int p, Scalar x,Scalar& L0,Scalar& L0_1,Scalar& L0_2);
	void legendre_gauss(int N, Scalar* xgl, Scalar* wgl);
	void legendre_gauss_lobatto(int N, Scalar* xgl, Scalar* wgl);
	void cardinal_basis(int i,int N, Scalar* xgl,Scalar* psi);
	void lagrange_basis_derivative(int i,int N, Scalar* xgl,Scalar* dpsi);
	void legendre_basis_derivative(int i,int N, Scalar* xgl,Scalar* dpsi);
	void init_poly();
	void init_basis();
	void init_geom();
	
	/*expand fields*/
	template<class type, ENTITY entity>
	void expand(const MeshField<type,entity>& cF) {
		if(DG::NPMAT) {
			Int block;
			if(entity == CELL) 
				block = NP;
			else if(entity == FACET) 
				block = NPF;
			
			for(int i = cF.size() - 1;i >= 0;i -= block) {
				Int ii = i / block;
				type C = cF[ii];
				for(Int j = 0; j < block;j++)
					cF[i - j] = C; 
			}
		}
	}
}

#endif