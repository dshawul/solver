#ifndef __TURBULENCE_H
#define __TURBULENCE_H

#include "field.h"
#include "solve.h"
/*
	 Description of RANS turbulence models
	 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	 Navier Stokes without source term:
	     d(rho*u)/dt + div(rho*uu) = -grad(p) + div(mu*gu)
	 RANS:
	     d(rho*U)/dt + div(rho*UU) + div(rho*u'u') = -grad(P) + div(mu*gU)
	     d(rho*U)/dt + div(rho*UU) = -grad(P) + div(mu*gU) - div(rho*u'u')
	     d(rho*U)/dt + div(rho*UU) = -grad(P) + div(V + R)
	 where Viscous (V) and Reynolds (R) stress tensors are
		 V =  mu*gU
	     R = -rho*u'u'
	 Boussinesq model for R:
	     Traceless(R) = 2 * emu * Traceless(S) 
		 where S = (gU + gUt) / 2
	     R - R_ii/3 = 2 * emu * (S - S_ii/3)
	     R = 2 * emu * (S - S_ii/3) + R_ii/3
	       = 2 * emu * ((gU + gUt)/2 - gU_ii/3) + R_ii/3
	       = emu * gU + emu * (gUt - 2/3*gUt_ii) + R_ii/3
	       = emu * gU + emu * dev(gUt,2) - 2/3*rho*k*I
	 Viscous and Reynolds stress together:
	     V + R = {mu * gU} + {emu * gU + emu * dev(gUt,2) - 2/3*rho*k*I}
	           = (mu + emu) * gU + emu * dev(gUt,2) - 2/3*rho*k*I
	           = ( eff_mu ) * gU + emu * dev(gUt,2) - 2/3*rho*k*I
	 Volume integrated V+R i.e force:
	     div(V + R) = div(eff_mu*gU) + div(emu * dev(gUt,2)) - div(2/3*rho*k*I)
	                     Implicit           Explicit          Absored in pressure 
	                                                          p_m = p + 2/3*k*rho
	 Final RANS equation after substituting div(V+R):
		 d(rho*U)/dt + div(rho*UU) = -grad(P) + div(V + R)
	     d(rho*U)/dt + div(rho*UU) = -grad(P_m) + div(eff_mu*gU) + div(emu * dev(gUt,2))
	 Since the k term is absorbed into the pressure gradient, we only need models for
	 turbulent diffusivity emu.
*/

/*
     Base turbulence model:
         This default class has no turbulence model so it is a laminar solver. 
         Only the viscous stress V is added to the NS equations. Turbulence models 
         derived from this class add a model for Reynold's stress R usually by solving 
         'turbulence transport' equations.
*/
struct Turbulence_Model {

	VectorCellField& U;
	ScalarFacetField& F;
	Scalar& rho;
	Scalar& nu;
	bool& Steady;

	Util::ParamList params;
	/*constructor*/
	Turbulence_Model(VectorCellField& tU,ScalarFacetField& tF,Scalar& trho,Scalar& tnu,bool& tSteady) :
		U(tU),
		F(tF),
		rho(trho),
		nu(tnu),
		Steady(tSteady),
		params("turbulence")
	{
	}
	/*overridable functions*/
	virtual void enroll() {};
	virtual void solve() {};
	virtual void addTurbulentStress(VectorMeshMatrix& M) {
		ScalarFacetField mu = rho * nu;
		M -= lap(U,mu);
	};
	/* V */
	STensorCellField getViscousStress() {
		STensorCellField V = 2 * rho * nu * sym(grad(U));
		return V;
	}
	/* R */
	virtual STensorCellField getReynoldsStress() {
		return STensor(0);
	}
	/* TKE */
	virtual ScalarCellField getK() {
		return Scalar(0);
	}
};
/*
 * Eddy viscosity models based on Boussinesq's assumption
 * that the action of Reynolds and Viscous stress are similar.
 */
struct EddyViscosity_Model : public Turbulence_Model {
	ScalarCellField eddy_mu; 
	enum Model {
		SMAGORNSKY,BALDWIN,KATO
	};
	enum WallModel {
		NONE,STANDARD,LAUNDER
	};
	Model modelType;
	WallModel wallModel;
	
	/*constructor*/
	EddyViscosity_Model(VectorCellField& tU,ScalarFacetField& tF,Scalar& trho,Scalar& tnu,bool& tSteady) :
		Turbulence_Model(tU,tF,trho,tnu,tSteady),
		modelType(SMAGORNSKY),
		wallModel(STANDARD)
	{
	}
	/*Register options*/
	virtual void enroll() {
		using namespace Util;
		Option* op = new Option(&modelType,3,
			"SMAGORNSKY","BALDWIN","KATO");
		params.enroll("modelType",op);
		Turbulence_Model::enroll();
	}
	/*eddy_mu*/
	virtual void calcEddyViscosity(const TensorCellField& gradU) = 0;

	/* V + R */
	virtual void addTurbulentStress(VectorMeshMatrix& M) {
		TensorCellField gradU = grad(U);
		calcEddyViscosity(gradU);
		setWallEddyMu();

		ScalarCellField eff_mu = eddy_mu + rho * nu;
		M -= lap(U,eff_mu);
		M -= div(eddy_mu * dev(trn(gradU),2));
	};
	/* R */
	virtual STensorCellField getReynoldsStress() {
		STensorCellField R = 2 * eddy_mu * dev(sym(grad(U))) - 
			     STensorCellField(Constants::I_ST) * (2 * rho * getK() / 3);
		return R;
	}
	/* S2 */
	ScalarCellField getS2(const TensorCellField& gradU) {
		ScalarCellField magS;
		if(modelType == SMAGORNSKY) {
			STensorCellField S = sym(gradU);
			magS = S & S;
		} else if(modelType == BALDWIN) {
			TensorCellField O = skw(gradU);
			magS = O & O;
		} else {
			STensorCellField S = sym(gradU);
			TensorCellField O = skw(gradU);
			magS = sqrt((S & S) * (O & O));
		}
		return (2 * magS);
	}
	/* Wall functions */
	void setWallEddyMu() {
		using namespace Mesh;
		BasicBCondition* bbc;
		forEach(AllBConditions,d) {
			bbc = AllBConditions[d];
			if(bbc->isWall && (bbc->fIndex == U.fIndex)) {
				IntVector& wall_faces = *bbc->bdry;
				LawOfWall& low = bbc->low;
				if(wall_faces.size()) {
					forEach(wall_faces,i) {
						applyWallFunction(wall_faces[i],low);
					}
				}
			}
		}
	}
	/*over-ridable*/
	virtual void applyWallFunction(Int f,LawOfWall& low) = 0;
};
/*
 * Base two equation K-X turbulence model
 */ 
struct KX_Model : public EddyViscosity_Model {
	/*model coefficients*/
	Scalar Cmu;
	Scalar SigmaK;
	Scalar SigmaX;
	Scalar C1x;
	Scalar C2x;

	Scalar k_UR;
	Scalar x_UR;

	/*turbulence fields*/
	ScalarCellField k;         
	ScalarCellField x;        
	ScalarCellField Pk;       

	/*constructor*/
	KX_Model(VectorCellField& tU,ScalarFacetField& tF,Scalar& trho,Scalar& tnu,bool& tSteady,const char* xname) :
		EddyViscosity_Model(tU,tF,trho,tnu,tSteady),
		k_UR(0.7),
		x_UR(0.7),
		k("k",READWRITE),
		x(xname,READWRITE)
	{
		wallModel = LAUNDER;
	}
	/*TKE*/
	virtual ScalarCellField getK() { return k; }
	/*Register options*/
	virtual void enroll() {
		using namespace Util;
		params.enroll("k_UR",&k_UR);
		params.enroll("x_UR",&x_UR);
		EddyViscosity_Model::enroll();
	}
	/* k-x model specific over-ridables*/
	virtual void calcEddyMu() = 0;
	virtual Scalar calcX(Scalar ustar,Scalar kappa,Scalar y) = 0;
	virtual Scalar getCmu(Int i) { 
		return Cmu; 
	}
	/* eddy viscosity*/
	virtual void calcEddyViscosity(const TensorCellField& gradU) {
		calcEddyMu();
		Pk = getS2(gradU) * eddy_mu;
	}
	/* wall function */
	virtual void applyWallFunction(Int f,LawOfWall& low) { 
		using namespace Mesh;
		Int c1 = gFO[f];
		Int c2 = gFN[f];

		/*calc ustar*/
		Scalar ustar;
		Scalar y = mag(unit(fN[f]) & (cC[c1] - cC[c2]));
		if(wallModel == STANDARD) {
			ustar = low.getUstar(nu,mag(U[c1]),y);
			k[c1] = pow(ustar,2) / sqrt(getCmu(c1));
		} else if(wallModel == LAUNDER) {
			ustar = pow(getCmu(c1),Scalar(0.25)) * sqrt(k[c1]);
		}
		x[c1] = calcX(ustar,low.kappa,y);

		/* calculate eddy viscosity*/
		Scalar yp = (ustar * y) / nu;
		Scalar up = low.getUp(ustar,nu,yp);                                      	
		eddy_mu[c1] = (rho * nu) * (yp / up - 1);

		/* turbulence generation and dissipation */
		if(wallModel == LAUNDER) {
			Scalar mag_dudy = mag((U[c2] - U[c1]) / y);
			Scalar mag_dudy_log = ustar / (low.kappa * y);
			Pk[c1] = (mag_dudy * mag_dudy_log) * eddy_mu[c1];
		}
	};
};

#endif