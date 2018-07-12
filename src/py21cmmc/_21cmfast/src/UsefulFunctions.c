// ----------------------------------------------------------------------------------------- //

// Taken from INIT_PARAMS.H

// ----------------------------------------------------------------------------------------- //



#define MIDDLE (user_params_ufunc->DIM/2)
#define D (unsigned long long)user_params_ufunc->DIM // the unsigned long long dimension
#define MID ((unsigned long long)MIDDLE)
#define VOLUME (user_params_ufunc->BOX_LEN*user_params_ufunc->BOX_LEN*user_params_ufunc->BOX_LEN) // in Mpc^3
#define DELTA_K (TWOPI/user_params_ufunc->BOX_LEN)
#define TOT_NUM_PIXELS ((unsigned long long)(D*D*D)) // no padding
#define TOT_FFT_NUM_PIXELS ((unsigned long long)(D*D*2llu*(MID+1llu)))
#define KSPACE_NUM_PIXELS ((unsigned long long)(D*D*(MID+1llu)))

// Define some useful macros

// for 3D complex array
#define C_INDEX(x,y,z)((unsigned long long)((z)+(MID+1llu)*((y)+D*(x))))

// for 3D real array with the FFT padding
#define R_FFT_INDEX(x,y,z)((unsigned long long)((z)+2llu*(MID+1llu)*((y)+D*(x))))

// for 3D real array with no padding
#define R_INDEX(x,y,z)((unsigned long long)((z)+D*((y)+D*(x))))


// ----------------------------------------------------------------------------------------- //

// Taken from ANAL_PARAMS.H

// ----------------------------------------------------------------------------------------- //



#define HII_D (unsigned long long) (user_params_ufunc->HII_DIM)
#define HII_MIDDLE (user_params_ufunc->HII_DIM/2)
#define HII_MID ((unsigned long long)HII_MIDDLE)

#define HII_TOT_NUM_PIXELS (unsigned long long)(HII_D*HII_D*HII_D)
#define HII_TOT_FFT_NUM_PIXELS ((unsigned long long)(HII_D*HII_D*2llu*(HII_MID+1llu)))
#define HII_KSPACE_NUM_PIXELS ((unsigned long long)(HII_D*HII_D*(HII_MID+1llu)))

// INDEXING MACROS //
// for 3D complex array
#define HII_C_INDEX(x,y,z)((unsigned long long)((z)+(HII_MID+1llu)*((y)+HII_D*(x))))
// for 3D real array with the FFT padding
#define HII_R_FFT_INDEX(x,y,z)((unsigned long long)((z)+2llu*(HII_MID+1llu)*((y)+HII_D*(x))))
// for 3D real array with no padding
#define HII_R_INDEX(x,y,z)((unsigned long long)((z)+HII_D*((y)+HII_D*(x))))



// ----------------------------------------------------------------------------------------- //

// Taken from COSMOLOGY.H

// ----------------------------------------------------------------------------------------- //



#define Ho  (double) (cosmo_params_ufunc->hlittle*3.2407e-18) // s^-1 at z=0
#define RHOcrit (double) ( (3.0*Ho*Ho / (8.0*PI*G)) * (CMperMPC*CMperMPC*CMperMPC)/Msun) // Msun Mpc^-3 ---- at z=0
#define RHOcrit_cgs (double) (3.0*Ho*Ho / (8.0*PI*G)) // g pcm^-3 ---- at z=0
#define No  (double) (RHOcrit_cgs*cosmo_params_ufunc->OMb*(1-global_params.Y_He)/m_p)  //  current hydrogen number density estimate  (#/cm^3)  ~1.92e-7
#define He_No (double) (RHOcrit_cgs*cosmo_params_ufunc->OMb*global_params.Y_He/(4.0*m_p)) //  current helium number density estimate
#define N_b0 (double) (No+He_No) // present-day baryon num density, H + He
#define f_H (double) (No/(No+He_No))  // hydrogen number fraction
#define f_He (double) (He_No/(No+He_No))  // helium number fraction

struct CosmoParams *cosmo_params_ufunc;
struct UserParams *user_params_ufunc;

void Broadcast_struct_global_UF(struct UserParams *user_params, struct CosmoParams *cosmo_params){
 
    cosmo_params_ufunc = cosmo_params;
    user_params_ufunc = user_params;
}



void filter_box(fftwf_complex *box, int RES, int filter_type, float R){
    int n_x, n_z, n_y, dimension,midpoint;
    float k_x, k_y, k_z, k_mag, kR;
    
//    printf("before = %e\n",box[C_INDEX(50, 50, 50)]);
    
    switch(RES) {
        case 0:
            dimension = user_params_ufunc->DIM;
            midpoint = MIDDLE;
            break;
        case 1:
            dimension = user_params_ufunc->HII_DIM;
            midpoint = HII_MIDDLE;
            break;
    }
    
    // loop through k-box
    for (n_x=dimension; n_x--;){
        if (n_x>midpoint) {k_x =(n_x-dimension) * DELTA_K;}
        else {k_x = n_x * DELTA_K;}
        
        for (n_y=dimension; n_y--;){
            if (n_y>midpoint) {k_y =(n_y-dimension) * DELTA_K;}
            else {k_y = n_y * DELTA_K;}
            
            for (n_z=(midpoint+1); n_z--;){
                k_z = n_z * DELTA_K;
                
                k_mag = sqrt(k_x*k_x + k_y*k_y + k_z*k_z);
                
                kR = k_mag*R; // real space top-hat
                
//                if(n_x == 50 && n_y == 50 && n_z == 50) {
//                    printf("n_x = %d n_y = %d n_z = %d k_x = %e k_y = %e k_z = %e k_mag = %e kR = %e\n",n_x,n_y,n_z,k_x,k_y,k_z,k_mag,kR);
//                }
                
                if (filter_type == 0){ // real space top-hat
                    if (kR > 1e-4){
//                        box[HII_C_INDEX(n_x, n_y, n_z)] *= 3.0*pow(kR, -3) * (sin(kR) - cos(kR)*kR);
//                        printf("n_x = %d n_y = %d n_z = %d HIRES_box = %e\n",n_x,n_y,n_z,box[C_INDEX(n_x, n_y, n_z)]);
                        if(RES==1) { box[HII_C_INDEX(n_x, n_y, n_z)] *= 3.0*pow(kR, -3) * (sin(kR) - cos(kR)*kR); }
                        if(RES==0) { box[C_INDEX(n_x, n_y, n_z)] *= 3.0*pow(kR, -3) * (sin(kR) - cos(kR)*kR); }
//                        printf("k_x = %e k_y = %e k_z = %e k_mag = %e kR = %e arg = %e HIRES_box = %e\n",k_x,k_y,k_z,k_mag,kR,3.0*pow(kR, -3) * (sin(kR) - cos(kR)*kR),box[C_INDEX(n_x, n_y, n_z)]);
                    }
                }
                else if (filter_type == 1){ // k-space top hat
                    kR *= 0.413566994; // equates integrated volume to the real space top-hat (9pi/2)^(-1/3)
                    if (kR > 1){
                        if(RES==1) { box[HII_C_INDEX(n_x, n_y, n_z)] = 0; }
                        if(RES==0) { box[C_INDEX(n_x, n_y, n_z)] = 0; }
                    }
                }
                else if (filter_type == 2){ // gaussian
                    kR *= 0.643; // equates integrated volume to the real space top-hat
                    if(RES==1) { box[HII_C_INDEX(n_x, n_y, n_z)] *= pow(E, -kR*kR/2.0); }
                    if(RES==0) { box[C_INDEX(n_x, n_y, n_z)] *= pow(E, -kR*kR/2.0); }
                }
                else{
                    if ( (n_x==0) && (n_y==0) && (n_z==0) )
                        fprintf(stderr, "HII_filter.c: Warning, filter type %i is undefined\nBox is unfiltered\n", filter_type);
                }
            }
        }
    } // end looping through k box
    
//    printf("after = %e\n",box[C_INDEX(50, 50, 50)]);
    
    return;
}

double MtoR(double M);
double RtoM(double R);
float TtoM(float z, float T, float mu);
double dicke(double z);
double dtdz(float z);
double ddickedt(double z);
double omega_mz(float z);
double Deltac_nonlinear(float z);


/* R in Mpc, M in Msun */
double MtoR(double M){
    
    // set R according to M<->R conversion defined by the filter type in ../Parameter_files/COSMOLOGY.H
    if (global_params.FILTER == 0) //top hat M = (4/3) PI <rho> R^3
        return pow(3*M/(4*PI*cosmo_params_ufunc->OMm*RHOcrit), 1.0/3.0);
    else if (global_params.FILTER == 1) //gaussian: M = (2PI)^1.5 <rho> R^3
        return pow( M/(pow(2*PI, 1.5) * cosmo_params_ufunc->OMm * RHOcrit), 1.0/3.0 );
    else // filter not defined
        fprintf(stderr, "No such filter = %i.\nResults are bogus.\n", global_params.FILTER);
    return -1;
}

/* R in Mpc, M in Msun */
double RtoM(double R){
    // set M according to M<->R conversion defined by the filter type in ../Parameter_files/COSMOLOGY.H
    if (global_params.FILTER == 0) //top hat M = (4/3) PI <rho> R^3
        return (4.0/3.0)*PI*pow(R,3)*(cosmo_params_ufunc->OMm*RHOcrit);
    else if (global_params.FILTER == 1) //gaussian: M = (2PI)^1.5 <rho> R^3
        return pow(2*PI, 1.5) * cosmo_params_ufunc->OMm*RHOcrit * pow(R, 3);
    else // filter not defined
        fprintf(stderr, "No such filter = %i.\nResults are bogus.\n", global_params.FILTER);
    return -1;
}

/*
 T in K, M in Msun, mu is mean molecular weight
 from Barkana & Loeb 2001
 
 SUPRESS = 0 for no radiation field supression;
 SUPRESS = 1 for supression (step function at z=z_ss, at v=v_zz)
 */
float TtoM(float z, float T, float mu){
    return 7030.97 / (cosmo_params_ufunc->hlittle) * sqrt( omega_mz(z) / (cosmo_params_ufunc->OMm*Deltac_nonlinear(z)) ) *
    pow( T/(mu * (1+z)), 1.5 );
    /*  if (!SUPRESS || (z >= z_re) ) // pre-reionization or don't worry about supression
     return 7030.97 / hlittle * sqrt( omega_mz(z) / (OMm*Deltac_nonlinear(z)) ) *
     pow( T/(mu * (1+z)), 1.5 );
     
     if (z >= z_ss) // self-shielding dominates, use T = 1e4 K
     return 7030.97 / hlittle * sqrt( omega_mz(z) / (OMm*Deltac_nonlinear(z)) ) *
     pow( 1.0e4 /(mu * (1+z)), 1.5 );
     
     // optically thin
     return 7030.97 / hlittle * sqrt( omega_mz(z) / (OMm*Deltac_nonlinear(z)) ) *
     pow( VcirtoT(v_ss, mu) /(mu * (1+z)), 1.5 );
     */
}


/* Physical (non-linear) overdensity at virialization (relative to critical density)
 i.e. answer is rho / rho_crit
 In Einstein de sitter model = 178
 (fitting formula from Bryan & Norman 1998) */
double Deltac_nonlinear(float z){
    double d;
    d = omega_mz(z) - 1.0;
    return 18*PI*PI + 82*d - 39*d*d;
}

/* Omega matter at redshift z */
double omega_mz(float z){
    return cosmo_params_ufunc->OMm*pow(1+z,3) / (cosmo_params_ufunc->OMm*pow(1+z,3) + cosmo_params_ufunc->OMl + global_params.OMr*pow(1+z,4) + global_params.OMk*pow(1+z, 2));
}


/*
 FUNCTION dicke(z)
 Computes the dicke growth function at redshift z, i.e. the z dependance part of sigma
 
 References: Peebles, "Large-Scale...", pg.53 (eq. 11.16). Includes omega<=1
 Nonzero Lambda case from Liddle et al, astro-ph/9512102, eqs. 6-8.
 and quintessence case from Wang et al, astro-ph/9804015
 
 Normalized to dicke(z=0)=1
 */
double dicke(double z){
    double omegaM_z, dick_z, dick_0, x, x_0;
    double tiny = 1e-4;

    if (fabs(cosmo_params_ufunc->OMm-1.0) < tiny){ //OMm = 1 (Einstein de-Sitter)
        return 1.0/(1.0+z);
    }
    else if ( (cosmo_params_ufunc->OMl > (-tiny)) && (fabs(cosmo_params_ufunc->OMl+cosmo_params_ufunc->OMm+global_params.OMr-1.0) < 0.01) && (fabs(global_params.wl+1.0) < tiny) ){
        //this is a flat, cosmological CONSTANT universe, with only lambda, matter and radiation
        //it is taken from liddle et al.
        omegaM_z = cosmo_params_ufunc->OMm*pow(1+z,3) / ( cosmo_params_ufunc->OMl + cosmo_params_ufunc->OMm*pow(1+z,3) + global_params.OMr*pow(1+z,4) );
        dick_z = 2.5*omegaM_z / ( 1.0/70.0 + omegaM_z*(209-omegaM_z)/140.0 + pow(omegaM_z, 4.0/7.0) );
        dick_0 = 2.5*cosmo_params_ufunc->OMm / ( 1.0/70.0 + cosmo_params_ufunc->OMm*(209-cosmo_params_ufunc->OMm)/140.0 + pow(cosmo_params_ufunc->OMm, 4.0/7.0) );
        return dick_z / (dick_0 * (1.0+z));
    }
    else if ( (global_params.OMtot < (1+tiny)) && (fabs(cosmo_params_ufunc->OMl) < tiny) ){ //open, zero lambda case (peebles, pg. 53)
        x_0 = 1.0/(cosmo_params_ufunc->OMm+0.0) - 1.0;
        dick_0 = 1 + 3.0/x_0 + 3*log(sqrt(1+x_0)-sqrt(x_0))*sqrt(1+x_0)/pow(x_0,1.5);
        x = fabs(1.0/(cosmo_params_ufunc->OMm+0.0) - 1.0) / (1+z);
        dick_z = 1 + 3.0/x + 3*log(sqrt(1+x)-sqrt(x))*sqrt(1+x)/pow(x,1.5);
        return dick_z/dick_0;
    }
    else if ( (cosmo_params_ufunc->OMl > (-tiny)) && (fabs(global_params.OMtot-1.0) < tiny) && (fabs(global_params.wl+1) > tiny) ){
        fprintf(stderr, "IN WANG\n");
        return -1;
    }
    
    fprintf(stderr, "No growth function!!! Output will be fucked up.");
    return -1;
}

/* function DTDZ returns the value of dt/dz at the redshift parameter z. */
double dtdz(float z){
    double x, dxdz, const1, denom, numer;
    x = sqrt( cosmo_params_ufunc->OMl/cosmo_params_ufunc->OMm ) * pow(1+z, -3.0/2.0);
    dxdz = sqrt( cosmo_params_ufunc->OMl/cosmo_params_ufunc->OMm ) * pow(1+z, -5.0/2.0) * (-3.0/2.0);
    const1 = 2 * sqrt( 1 + cosmo_params_ufunc->OMm/cosmo_params_ufunc->OMl ) / (3.0 * Ho) ;
    
    numer = dxdz * (1 + x*pow( pow(x,2) + 1, -0.5));
    denom = x + sqrt(pow(x,2) + 1);
    return (const1 * numer / denom);
}

/* Time derivative of the growth function at z */
double ddickedt(double z){
    float dz = 1e-10;
    double omegaM_z, ddickdz, dick_0, x, x_0, domegaMdz;
    double tiny = 1e-4;
    
    return (dicke(z+dz)-dicke(z))/dz/dtdz(z); // lazy non-analytic form getting
    
    if (fabs(cosmo_params_ufunc->OMm-1.0) < tiny){ //OMm = 1 (Einstein de-Sitter)
        return -pow(1+z,-2)/dtdz(z);
    }
    else if ( (cosmo_params_ufunc->OMl > (-tiny)) && (fabs(cosmo_params_ufunc->OMl+cosmo_params_ufunc->OMm+global_params.OMr-1.0) < 0.01) && (fabs(global_params.wl+1.0) < tiny) ){
        //this is a flat, cosmological CONSTANT universe, with only lambda, matter and radiation
        //it is taken from liddle et al.
        omegaM_z = cosmo_params_ufunc->OMm*pow(1+z,3) / ( cosmo_params_ufunc->OMl + cosmo_params_ufunc->OMm*pow(1+z,3) + global_params.OMr*pow(1+z,4) );
        domegaMdz = omegaM_z*3/(1+z) - cosmo_params_ufunc->OMm*pow(1+z,3)*pow(cosmo_params_ufunc->OMl + cosmo_params_ufunc->OMm*pow(1+z,3) + global_params.OMr*pow(1+z,4), -2) * (3*cosmo_params_ufunc->OMm*(1+z)*(1+z) + 4*global_params.OMr*pow(1+z,3));
        dick_0 = cosmo_params_ufunc->OMm / ( 1.0/70.0 + cosmo_params_ufunc->OMm*(209-cosmo_params_ufunc->OMm)/140.0 + pow(cosmo_params_ufunc->OMm, 4.0/7.0) );
        
        ddickdz = (domegaMdz/(1+z)) * (1.0/70.0*pow(omegaM_z,-2) + 1.0/140.0 + 3.0/7.0*pow(omegaM_z, -10.0/3.0)) * pow(1.0/70.0/omegaM_z + (209.0-omegaM_z)/140.0 + pow(omegaM_z, -3.0/7.0) , -2);
        ddickdz -= pow(1+z,-2)/(1.0/70.0/omegaM_z + (209.0-omegaM_z)/140.0 + pow(omegaM_z, -3.0/7.0));
        
        return ddickdz / dick_0 / dtdz(z);
    }
    
    fprintf(stderr, "No growth function!!! Output will be fucked up.");
    return -1;
}

/* returns the hubble "constant" (in 1/sec) at z */
double hubble(float z){
    return Ho*sqrt(cosmo_params_ufunc->OMm*pow(1+z,3) + global_params.OMr*pow(1+z,4) + cosmo_params_ufunc->OMl);
}


/* returns hubble time (in sec), t_h = 1/H */
double t_hubble(float z){
    return 1.0/hubble(z);
}



