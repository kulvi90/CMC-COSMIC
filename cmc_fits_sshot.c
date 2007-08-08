#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <ctype.h>
#include <float.h>
#include <string.h>
#include <fitsio.h>
#include <gsl/gsl_rng.h>
#include <errno.h>
#include "cmc.h"
#include "cmc_vars.h"

#define ALLOC_TSTUFF \
	ttype = (char **) saf_malloc(tfields*sizeof(char *)); \
	tform = (char **) saf_malloc(tfields*sizeof(char *)); \
	tunit = (char **) saf_malloc(tfields*sizeof(char *)); \
	for(i=0; i<tfields; i++){ \
		ttype[i] = (char *) saf_malloc(1024*sizeof(char)); \
		tform[i] = (char *) saf_malloc(1024*sizeof(char)); \
		tunit[i] = (char *) saf_malloc(1024*sizeof(char)); \
	} \

#define FREE_TSTUFF \
	for(i=0;i<tfields;i++){ \
		free(ttype[i]); \
		free(tform[i]); \
		free(tunit[i]); \
	} \
	free(ttype); free(tform); free(tunit); 

/* Using DBL_MAX in FITS files is not possible with glibc in GNU/Linux 
 * so we use an artificial large number */
#define LARGE_DISTANCE 1.0e40

/* exit with the error message and the status provided */
static void ext_err(char *mess, int status){
	eprintf("%s\n", mess);
	exit_cleanly(status);
}

/* safe malloc */
static void *saf_malloc(size_t size){
	void *dummy;

	if((dummy=malloc(size)) == NULL){
		ext_err("Memory allocation failed.", 102);
	} 
	return dummy;
}

void write_restart_param(fitsfile *fptr, gsl_rng *rng){
	char extname[1024];	/* extension name */
	int tfields;       	/* number of columns */
	long nrows;	
	long firstrow, firstelem;
	char **ttype, **tform, **tunit;
	int status;
	long i;
	struct rng_t113_state rng_st;
	double *dbl_arr;
	long *lng_arr;
	int *int_arr;
	char *rng_st_ptr;
	size_t rng_size=gsl_rng_size(rng); 
	double *dvar;
	int no_of_doub;


	status = 0;
	sprintf(extname,"SF_RESTART_PARAM");
	tfields = 3; 
	no_of_doub = 61;
	dvar=(double *) malloc(no_of_doub*sizeof(double));

	nrows = MASS_PC_COUNT;
	if (rng_size>(unsigned long) nrows){
		nrows = rng_size;
	}
	if (no_of_doub>nrows){
		nrows = no_of_doub;
	}
	dbl_arr = (double *) saf_malloc(nrows*sizeof(double));
	lng_arr = (long *) saf_malloc(nrows*sizeof(long));
	int_arr = (int *) saf_malloc(nrows*sizeof(int));
	firstrow  = 1;  /* first row in table to write   */
	firstelem = 1;  /* first element in row          */

	ALLOC_TSTUFF
	sprintf(ttype[0],"Mass_PC");
	sprintf(tform[0],"1D");
	sprintf(tunit[0],"Internal");
	sprintf(ttype[1],"RNG State");
	sprintf(tform[1],"1I");
	sprintf(tunit[1],"Internal");
	sprintf(ttype[2],"Variables");
	sprintf(tform[2],"1D");
	sprintf(tunit[2],"Various");

	fits_create_tbl(fptr, BINARY_TBL, nrows, tfields, ttype, tform,
              tunit, extname, &status);
	/* variables related to progress */
	fits_write_key(fptr, TDOUBLE, "Time", &TotalTime, 
			"Age of cluster", &status);
	fits_write_key(fptr, TLONG, "Step", &tcount, 
			"Iteration Step", &status);
	cmc_fits_printerror(status);
	/* random number state */
	get_rng_t113(&rng_st);
	fits_write_comment(fptr,
		"random number state", &status);
	fits_write_key(fptr, TULONG, "RNG_Z1", &(rng_st.z1), 
			"RNG STATE Z1", &status);
	fits_write_key(fptr, TULONG, "RNG_Z2", &(rng_st.z2), 
			"RNG STATE Z2", &status);
	fits_write_key(fptr, TULONG, "RNG_Z3", &(rng_st.z3), 
			"RNG STATE Z3", &status);
	fits_write_key(fptr, TULONG, "RNG_Z4", &(rng_st.z4), 
			"RNG STATE Z4", &status);
	cmc_fits_printerror(status);
	/* Total Energy in various forms */
	fits_write_comment(fptr,
		"Total Energy in various forms", &status);
	fits_write_key(fptr, TDOUBLE, "Etotal", &(Etotal.tot), 
			"Total Energy of Cluster", &status);
	fits_write_key(fptr, TDOUBLE, "EtotalN", &(Etotal.New), 
			"Total Energy of Cluster", &status);
	fits_write_key(fptr, TDOUBLE, "EtotalI", &(Etotal.ini), 
			"Initial Total Energy", &status);
	fits_write_key(fptr, TDOUBLE, "KEtotal", &(Etotal.K), 
			"Total Kinetic Energy", &status);
	fits_write_key(fptr, TDOUBLE, "PEtotal", &(Etotal.P), 
			"Total Potential Energy", &status);
	fits_write_key(fptr, TDOUBLE, "Eintot", &(Etotal.Eint), 
			"Total Intermediate Energy", &status);
	fits_write_key(fptr, TDOUBLE, "Ebtotal", &(Etotal.Eb), 
			"Total binary Energy", &status);
	cmc_fits_printerror(status);
	/* various N's of cluster */
	fits_write_comment(fptr,
		"It is not clear what the following different N's do", &status);
	fits_write_key(fptr, TLONG, "NMAX", &(clus.N_MAX), 
			"Number of stars in cluster (bound)", &status);
	fits_write_key(fptr, TLONG, "NMAXN", &(clus.N_MAX_NEW), 
			"Number of stars in cluster ", &status);
	fits_write_key(fptr, TLONG, "NSTAR", &(clus.N_STAR), 
			"Number of stars in cluster (initial)", &status);
	fits_write_key(fptr, TLONG, "NSTARN", &(clus.N_STAR_NEW), 
			"Number of stars in cluster (all)", &status);
	fits_write_key(fptr, TLONG, "NBINARY", &(clus.N_BINARY), 
			"Number of binaries in cluster", &status);
	cmc_fits_printerror(status);
	/* Input file parameters */
	fits_write_comment(fptr,
		"Input file parameters", &status);
	fits_write_key(fptr, TLONG, "NSTDIM", &(N_STAR_DIM), 
			"N_STAR_DIM", &status);
	fits_write_key(fptr, TLONG, "NBIDIM", &(N_BIN_DIM), 
			"N_BIN_DIM", &status);
	fits_write_key(fptr, TLONG, "TMXCNT", &(T_MAX_COUNT), 
			"T_MAX_COUNT", &status);
	fits_write_key(fptr, TLONG, "MPCCNT", &(MASS_PC_COUNT), 
			"MASS_PC_COUNT", &status);
	fits_write_key(fptr, TLONG, "STEVOL", &(STELLAR_EVOLUTION), 
			"STELLAR_EVOLUTION", &status);
	fits_write_key(fptr, TLONG, "SSCOLL", &(SS_COLLISION), 
			"SS_COLLISION", &status);
	fits_write_key(fptr, TLONG, "ECONS", &(E_CONS), 
			"E_CONS", &status);
	cmc_fits_printerror(status);
	/* IDUM is unnecessary to save, since RNG state is restored */
	fits_write_key(fptr, TLONG, "PERTUR", &(PERTURB), 
			"PERTURB", &status);
	fits_write_key(fptr, TLONG, "RELAX", &(RELAXATION), 
			"RELAXATION", &status);
	fits_write_key(fptr, TLONG, "NCNSTR", &(NUM_CENTRAL_STARS), 
			"NUM_CENTRAL_STARS", &status);
	fits_write_key(fptr, TDOUBLE, "TPRSTP", &(T_PRINT_STEP), 
			"T_PRINT_STEP", &status);
	fits_write_key(fptr, TDOUBLE, "TMAX", &(T_MAX), 
			"T_MAX", &status);
	fits_write_key(fptr, TDOUBLE, "TSEMAX", &(THETASEMAX), 
			"THETASEMAX", &status);
	fits_write_key(fptr, TDOUBLE, "TENEDI", &(TERMINAL_ENERGY_DISPLACEMENT), 
			"TERMINAL_ENERGY_DISPLACEMENT", &status);
	fits_write_key(fptr, TDOUBLE, "RMAX", &(R_MAX), 
			"R_MAX", &status);
	/* fits_write_key(fptr, TDOUBLE, "MINLR", &(MIN_LAGRANGIAN_RADIUS), 
	   "MIN_LAGRANGIAN_RADIUS", &status); */
	fits_write_key(fptr, TDOUBLE, "DTFACT", &(DT_FACTOR), 
	   "DT_FACTOR", &status); 
	fits_write_key(fptr, TDOUBLE, "MEGAYR", &(MEGA_YEAR), 
			"MEGA_YEAR", &status);
	fits_write_key(fptr, TDOUBLE, "SMDYN", &(SOLAR_MASS_DYN), 
			"SOLAR_MASS_DYN", &status);
	fits_write_key(fptr, TDOUBLE, "METZ", &(METALLICITY), 
			"METALLICITY", &status);
	fits_write_key(fptr, TDOUBLE, "WINFAC", &(WIND_FACTOR), 
			"WIND_FACTOR", &status);
	fits_write_key(fptr, TDOUBLE, "MMIN", &(MMIN), 
			"MMIN", &status);
	fits_write_key(fptr, TDOUBLE, "MINR", &(MINIMUM_R), 
			"MINIMUM_R", &status);
	fits_write_key(fptr, TDOUBLE, "GAMMA", &(GAMMA), 
			"GAMMA", &status);
	fits_write_key(fptr, TDOUBLE, "CENMAS", &(cenma.m), 
			"CentralMass_mass", &status);
	fits_write_key(fptr, TDOUBLE, "CENMAE", &(cenma.E), 
			"CentralMass_energy", &status);
	fits_write_key(fptr, TINT, "BINSIN", &(BINSINGLE), 
			"BINSINGLE", &status);
	fits_write_key(fptr, TINT, "BINBIN", &(BINBIN), 
			"BINBIN", &status);
	cmc_fits_printerror(status);
	/* variables related to tidal truncation */
	fits_write_comment(fptr,
		"Tidal Truncation parameters", &status);
	fits_write_key(fptr, TDOUBLE, "MAXR", &max_r, 
			"max_r", &status);
	fits_write_key(fptr, TDOUBLE, "RTIDAL", &Rtidal, 
			"Rtidal", &status);
	fits_write_key(fptr, TDOUBLE, "TMLOSS", &TidalMassLoss, 
			"TidalMassLoss", &status);
	fits_write_key(fptr, TDOUBLE, "ORBR", &orbit_r, 
			"orbit_r", &status);
	fits_write_key(fptr, TDOUBLE, "OTMLOS", &OldTidalMassLoss, 
			"OldTidalMassLoss", &status);
	fits_write_key(fptr, TDOUBLE, "DTMLOS", &DTidalMassLoss, 
			"DTidalMassLoss", &status);
	fits_write_key(fptr, TDOUBLE, "PREVDT", &Prev_Dt, 
			"Prev_Dt", &status);
	fits_write_key(fptr, TDOUBLE, "ETIDAL", &Etidal, 
			"Etidal", &status);
	fits_write_key(fptr, TDOUBLE, "CLGAMR", &clus_gal_mass_ratio, 
			"Cluster to Galaxy Mass Ratio", &status);
	fits_write_key(fptr, TDOUBLE, "DISGLC", &dist_gal_center, 
			"Distance to Galactic Center", &status);
	cmc_fits_printerror(status);
	/* variables related to binaries */
	fits_write_comment(fptr,
		"Binary parameters", &status);
	fits_write_key(fptr, TLONG, "NB", &N_b, 
			"N_b", &status);
	fits_write_key(fptr, TLONG, "NBB", &N_bb, 
			"N_bb", &status);
	fits_write_key(fptr, TLONG, "NBS", &N_bs, 
			"N_bs", &status);
	fits_write_key(fptr, TDOUBLE, "MB", &M_b, 
			"M_b", &status);
	fits_write_key(fptr, TDOUBLE, "EB", &E_b, 
			"E_b", &status);
	cmc_fits_printerror(status);
	/* variables related to core  */
	fits_write_comment(fptr,
		"Core parameters", &status);
	fits_write_key(fptr, TDOUBLE, "NCORE", &N_core, 
			"N_core", &status);
	fits_write_key(fptr, TDOUBLE, "TRC", &Trc, 
			"Trc", &status);
	fits_write_key(fptr, TDOUBLE, "COREDE", &rho_core, 
			"rho_core", &status);
	fits_write_key(fptr, TDOUBLE, "COREV", &v_core, 
			"v_core", &status);
	fits_write_key(fptr, TDOUBLE, "CORER", &core_radius, 
			"core_radius", &status);
	cmc_fits_printerror(status);
	/* variables related to escaped stars  */
	fits_write_comment(fptr,
		"Escaped stars' parameters", &status);
	fits_write_key(fptr, TDOUBLE, "EESC", &Eescaped, 
			"Eescaped", &status);
	fits_write_key(fptr, TDOUBLE, "JESC", &Jescaped, 
			"Jescaped", &status);
	fits_write_key(fptr, TDOUBLE, "EINESC", &Eintescaped, 
			"Escaped Stars Int Energy", &status);
	fits_write_key(fptr, TDOUBLE, "EBESC", &Ebescaped, 
			"Escaped Binaries Energy", &status);
	cmc_fits_printerror(status);
	/* Total mass and co.  */
	fits_write_comment(fptr,
		"Total mass and co.", &status);
	fits_write_key(fptr, TDOUBLE, "INMTOT", &initial_total_mass, 
			"initial_total_mass", &status);
	fits_write_key(fptr, TDOUBLE, "MTOTAL", &Mtotal, 
			"Mtotal", &status);
	cmc_fits_printerror(status);
	/* everything else except arrays */
	fits_write_comment(fptr,
		"Everything else", &status);
	/* integers */
	fits_write_key(fptr, TINT, "SEFCNT", &se_file_counter, 
			"se_file_counter", &status);
	cmc_fits_printerror(status);
	/* long integers */
	fits_write_key(fptr, TLONG, "TCOUNT", &tcount, 
			"tcount", &status);
	fits_write_key(fptr, TLONG, "ECHECK", &Echeck, 
			"Echeck", &status);
	fits_write_key(fptr, TLONG, "SNAPNO", &snap_num, 
			"snap_num", &status);
	fits_write_key(fptr, TLONG, "STCNT", &StepCount, 
			"StepCount", &status);
	cmc_fits_printerror(status);
	/* doubles */
	fits_write_key(fptr, TDOUBLE, "DECORS", &rho_core_single, 
			"rho_core_single", &status);
	fits_write_key(fptr, TDOUBLE, "DECORB", &rho_core_bin, 
			"rho_core_bin", &status);
	fits_write_key(fptr, TDOUBLE, "RHSIN", &rh_single, 
			"rh_single", &status);
	fits_write_key(fptr, TDOUBLE, "RHBIN", &rh_binary, 
			"rh_binary", &status);
	fits_write_key(fptr, TDOUBLE, "DT", &Dt, 
			"Dt", &status);
	fits_write_key(fptr, TDOUBLE, "S2BETA", &Sin2Beta, 
			"Sin2Beta", &status);
	fits_write_key(fptr, TDOUBLE, "MADHOC", &madhoc, 
			"madhoc", &status);
	cmc_fits_printerror(status);
	/* units */
	fits_write_key(fptr, TDOUBLE, "UNITT", &(units.t), 
			"Time unit", &status);
	fits_write_key(fptr, TDOUBLE, "UNITM", &(units.m), 
			"Mass unit", &status);
	fits_write_key(fptr, TDOUBLE, "UNITL", &(units.l), 
			"Length unit", &status);
	fits_write_key(fptr, TDOUBLE, "UNITE", &(units.E), 
			"Energy unit", &status);
	fits_write_key(fptr, TDOUBLE, "UNITSM", &(units.mstar), 
			"Star mass unit", &status);
	cmc_fits_printerror(status);
	/* it may be necessary to save and restore central.* variables */

	i = 0;
	/* variables related to progress */
	dvar[i++]  = TotalTime;
	/* Total Energy in various forms */
	dvar[i++]  = Etotal.tot;
	dvar[i++]  = Etotal.New;
	dvar[i++]  = Etotal.ini;
	dvar[i++]  = Etotal.K;
	dvar[i++]  = Etotal.P;
	dvar[i++]  = Etotal.Eint;
	dvar[i++]  = Etotal.Eb;
	/* Input file parameters */
	dvar[i++]  = T_PRINT_STEP;
	dvar[i++]  = T_MAX;
	dvar[i++] = THETASEMAX;
	dvar[i++] = TERMINAL_ENERGY_DISPLACEMENT;
	dvar[i++] = R_MAX;
	/* dvar[i++] = MIN_LAGRANGIAN_RADIUS; */
	dvar[i++] = DT_FACTOR;
	dvar[i++] = MEGA_YEAR;
	dvar[i++] = SOLAR_MASS_DYN;
	dvar[i++] = METALLICITY;
	dvar[i++] = WIND_FACTOR;
	dvar[i++] = MMIN;
	dvar[i++] = MINIMUM_R;
	dvar[i++] = GAMMA;
	dvar[i++] = cenma.m;
	dvar[i++] = cenma.E;
	/* variables related to tidal truncation */
	dvar[i++] = max_r;
	dvar[i++] = Rtidal;
	dvar[i++] = TidalMassLoss;
	dvar[i++] = orbit_r;
	dvar[i++] = OldTidalMassLoss;
	dvar[i++] = DTidalMassLoss;
	dvar[i++] = Prev_Dt;
	dvar[i++] = Etidal;
	dvar[i++] = clus_gal_mass_ratio;
	dvar[i++] = dist_gal_center;
	/* variables related to binaries */
	dvar[i++] = M_b;
	dvar[i++] = E_b;
	/* variables related to core  */
	dvar[i++] = N_core;
	dvar[i++] = Trc;
	dvar[i++] = rho_core;
	dvar[i++] = v_core;
	dvar[i++] = core_radius;
	/* variables related to escaped stars  */
	dvar[i++] = Eescaped;
	dvar[i++] = Jescaped;
	dvar[i++] = Eintescaped;
	dvar[i++] = Ebescaped;
	/* Total mass and co.  */
	dvar[i++] = initial_total_mass;
	dvar[i++] = Mtotal;
	/* everything else except arrays */
	dvar[i++] = rho_core_single;
	dvar[i++] = rho_core_bin;
	dvar[i++] = rh_single;
	dvar[i++] = rh_binary;
	dvar[i++] = Dt;
	dvar[i++] = Sin2Beta;
	dvar[i++] = madhoc;
	/* units */
	dvar[i++] = units.t;
	dvar[i++] = units.m;
	dvar[i++] = units.l;
	dvar[i++] = units.E;
	dvar[i++] = units.mstar;

	for(i=0; i<nrows; i++){
		dbl_arr[i] = 0.0;
	}
	for(i=0; i<MASS_PC_COUNT; i++){
		dbl_arr[i] = mass_pc[i];
	}
	fits_write_col(fptr, TDOUBLE, 1, firstrow, firstelem, nrows,
			dbl_arr, &status);
	cmc_fits_printerror(status);
	
	rng_st_ptr = (char *) rng->state;
	for(i=0; i<nrows; i++){
		int_arr[i] = 0;
	}
	for(i=0; i<(long) rng_size; i++){
		/* XXX I am not sure the below casting works !!! (ato) */
		int_arr[i] += (int) rng_st_ptr[i];
	}
	fits_write_col(fptr, TINT, 2, firstrow, firstelem, nrows,
			int_arr, &status);
	cmc_fits_printerror(status);
	
	for(i=0; i<nrows; i++){
		dbl_arr[i] = 0.0;
	}
	for(i=0; i<no_of_doub; i++){
		dbl_arr[i] = dvar[i];
	}
	fits_write_col(fptr, TDOUBLE, 3, firstrow, firstelem, nrows,
			dbl_arr, &status);
	cmc_fits_printerror(status);
	FREE_TSTUFF
	free(dbl_arr); free(lng_arr); free(int_arr); free(dvar);

}

void write_ss_dyn_param(fitsfile *fptr){
	char extname[1024];	/* extension name */
	int tfields;       	/* number of columns */
	long nrows;	
	long firstrow, firstelem;
	char **ttype, **tform, **tunit;
	int status;

	long i, N;
	double *dbl_arr;
	long *lng_arr;

	status = 0;

	sprintf(extname,"SS_DYN_PARAM");
     	N = clus.N_STAR_NEW;
	tfields = 9; nrows = N+2;
	firstrow  = 1;  /* first row in table to write   */
	firstelem = 1;  /* first element in row          */
	dbl_arr = (double *) saf_malloc((N+2)*sizeof(double));
	lng_arr = (long *) saf_malloc((N+2)*sizeof(long));

	ALLOC_TSTUFF
	sprintf(ttype[0],"Mass");
	sprintf(tform[0],"1D");
	sprintf(tunit[0],"Nbody");
	sprintf(ttype[1],"Position");
	sprintf(tform[1],"1D");
	sprintf(tunit[1],"Nbody");
	sprintf(ttype[2],"Vr");
	sprintf(tform[2],"1D");
	sprintf(tunit[2],"Nbody");
	sprintf(ttype[3],"Vt");
	sprintf(tform[3],"1D");
	sprintf(tunit[3],"Nbody");
	sprintf(ttype[4],"Pericenter");
	sprintf(tform[4],"1D");
	sprintf(tunit[4],"Nbody");
	sprintf(ttype[5],"Binary index");
	sprintf(tform[5],"1J");
	sprintf(tunit[5],"Index");
	sprintf(ttype[6],"Interaction Flag");
	sprintf(tform[6],"1J");
	sprintf(tunit[6],"Flag");
	sprintf(ttype[7],"ID number");
	sprintf(tform[7],"1J");
	sprintf(tunit[7],"Index");
	sprintf(ttype[8],"Sphi");
	sprintf(tform[8],"1D");
	sprintf(tunit[8],"Nbody");

	fits_create_tbl(fptr, BINARY_TBL, nrows, tfields, ttype, tform,
                tunit, extname, &status);
	fits_write_key(fptr, TLONG, "NSTAR", &(clus.N_STAR), 
			"No of Stars (initial)", &status);
	fits_write_key(fptr, TLONG, "NSTARN", &(clus.N_STAR_NEW), 
			"No of Stars (all)", &status);
	fits_write_key(fptr, TLONG, "NMAX", &(clus.N_MAX), 
			"No of Stars (bound)", &status);
	fits_write_key(fptr, TLONG, "NMAXN", &(clus.N_MAX_NEW), 
			"No of Stars (bound, new)", &status);

	/* mass */
	for(i=0;i<=N+1;i++){
		dbl_arr[i] = star[i].m;
	}
	fits_write_col(fptr, TDOUBLE, 1, firstrow, firstelem, nrows, dbl_arr,
                   &status);
	/* position */
	for(i=0;i<=N+1;i++){
		dbl_arr[i] = star[i].r;
	}
	fits_write_col(fptr, TDOUBLE, 2, firstrow, firstelem, nrows, dbl_arr,
                   &status);
	/* vr */
	for(i=0;i<=N+1;i++){
		dbl_arr[i] = star[i].vr;
	}
	fits_write_col(fptr, TDOUBLE, 3, firstrow, firstelem, nrows, dbl_arr,
                   &status);
	/* vt */
	for(i=0;i<=N+1;i++){
		dbl_arr[i] = star[i].vt;
	}
	fits_write_col(fptr, TDOUBLE, 4, firstrow, firstelem, nrows, dbl_arr,
                   &status);
	/* r_peri */
	for(i=0;i<=N+1;i++){
		dbl_arr[i] = star[i].r_peri;
	}
	fits_write_col(fptr, TDOUBLE, 5, firstrow, firstelem, nrows, dbl_arr,
                   &status);

	/* binindex */
	for(i=0;i<=N+1;i++){
		lng_arr[i] = star[i].binind;
	}
	fits_write_col(fptr, TLONG, 6, firstrow, firstelem, nrows, lng_arr,
                   &status);

	/* interacted? */
	for(i=0;i<=N+1;i++){
		lng_arr[i] = star[i].interacted;
	}
	fits_write_col(fptr, TLONG, 7, firstrow, firstelem, nrows, lng_arr,
                   &status);

	/* id */
	for(i=0;i<=N+1;i++){
		lng_arr[i] = star[i].id;
	}
	fits_write_col(fptr, TLONG, 8, firstrow, firstelem, nrows, lng_arr, &status);
	/* potential */
	for(i=0;i<=N+1;i++){
		dbl_arr[i] = star[i].phi;
	}
	fits_write_col(fptr, TDOUBLE, 9, firstrow, firstelem, nrows, dbl_arr,
                   &status);

	FREE_TSTUFF
	free(dbl_arr); free(lng_arr);

	cmc_fits_printerror(status);
}

void write_ss_se_param(fitsfile *fptr){
	char extname[1024];	/* extension name */
	int tfields;       	/* number of columns */
	long nrows;	
	long firstrow, firstelem;
	char **ttype, **tform, **tunit;
	int status;

	long i, N;
	double *dbl_arr;
	int *int_arr;

	status = 0;

	sprintf(extname,"SS_SE_PARAM");
     	N = clus.N_STAR;
	tfields = 3; nrows = N;
	firstrow  = 1;  /* first row in table to write   */
	firstelem = 1;  /* first element in row          */
	dbl_arr = (double *) saf_malloc((N+2)*sizeof(double));
	int_arr = (int *) saf_malloc((N+2)*sizeof(int));

	ALLOC_TSTUFF
	sprintf(ttype[0],"Radius");
	sprintf(tform[0],"1D");
	sprintf(tunit[0],"Solar");
	sprintf(ttype[1],"Mass");
	sprintf(tform[1],"1D");
	sprintf(tunit[1],"Solar");
	sprintf(ttype[2],"Type");
	sprintf(tform[2],"1I");
	sprintf(tunit[2],"Internal");
	
	fits_create_tbl(fptr, BINARY_TBL, nrows, tfields, ttype, tform,
                tunit, extname, &status);

	/* radius */
	dbl_arr[0] = 0.0;
	for(i=1;i<=N;i++){
		dbl_arr[i] = star[i].rad;
	}
	dbl_arr[N+1] = 0.0;
	fits_write_col(fptr, TDOUBLE, 1, firstrow, firstelem, nrows, dbl_arr,
                   &status);
	/* mass */
	dbl_arr[0] = 0.0;
	for(i=1;i<=N;i++){
		dbl_arr[i] = star[i].mass;
	}
	dbl_arr[N+1] = 0.0;
	fits_write_col(fptr, TDOUBLE, 2, firstrow, firstelem, nrows, dbl_arr,
                   &status);
	/* type */
	int_arr[0] = 0;
	for(i=1;i<=N;i++){
		int_arr[i] = star[i].k;
	}
	int_arr[N+1] = 0;
	fits_write_col(fptr, TINT, 3, firstrow, firstelem, nrows, int_arr,
                   &status);

	FREE_TSTUFF
	free(dbl_arr); free(int_arr);

	cmc_fits_printerror(status);
}

void write_basic_info(fitsfile *fptr){
	char extname[1024];	/* extension name */
	int tfields;       	/* number of columns */
	long nrows;	
	int is_snapshot;
	int status;

	status = 0;
	sprintf(extname,"META_INFO");
	tfields = 0; nrows = 0;		/* this extension has table */

	fits_create_tbl(fptr, BINARY_TBL, nrows, tfields, NULL, NULL,
                NULL, extname, &status);

	is_snapshot = 1;
	fits_write_key(fptr, TINT, "IS_SS", &is_snapshot, 
			"Is this a snapshot", &status);
	cmc_fits_printerror(status);
}

void write_bs_dyn_param(fitsfile *fptr){
	char extname[1024];	/* extension name */
	int tfields;       	/* number of columns */
	long nrows;	
	long firstrow, firstelem;
	char **ttype, **tform, **tunit;
	int status;

	long i, N;
	double *dbl_arr;
	long *lng_arr;
	int *int_arr;

	status = 0;

	sprintf(extname,"BS_DYN_PARAM");
	N = N_BIN_DIM;
	tfields = 11; nrows = N;
	firstrow  = 1;  /* first row in table to write   */
	firstelem = 1;  /* first element in row          */
	lng_arr = (long *) saf_malloc(N*sizeof(long));
	dbl_arr = (double *) saf_malloc(N*sizeof(double));
	int_arr = (int *) saf_malloc(N*sizeof(int));

	ALLOC_TSTUFF
	sprintf(ttype[0],"ID1");
	sprintf(tform[0],"1J");
	sprintf(tunit[0],"Index");
	
	sprintf(ttype[1],"ID2");
	sprintf(tform[1],"1J");
	sprintf(tunit[1],"Index");

	sprintf(ttype[2],"Rad1");
	sprintf(tform[2],"1D");
	sprintf(tunit[2],"Unknown"); /* FIXME it should be known though! */

	sprintf(ttype[3],"Rad2");
	sprintf(tform[3],"1D");
	sprintf(tunit[3],"Unknown");

	sprintf(ttype[4],"Mass1");
	sprintf(tform[4],"1D");
	sprintf(tunit[4],"Unknown");

	sprintf(ttype[5],"Mass2");
	sprintf(tform[5],"1D");
	sprintf(tunit[5],"Unknown");

	sprintf(ttype[6],"Eint1");
	sprintf(tform[6],"1D");
	sprintf(tunit[6],"Unknown");

	sprintf(ttype[7],"Eint2");
	sprintf(tform[7],"1D");
	sprintf(tunit[7],"Unknown");

	sprintf(ttype[8],"SMajAxis");
	sprintf(tform[8],"1D");
	sprintf(tunit[8],"Unknown");

	sprintf(ttype[9],"Eccentricity");
	sprintf(tform[9],"1D");
	sprintf(tunit[9],"Unknown");

	sprintf(ttype[10],"Inuse");
	sprintf(tform[10],"1I");
	sprintf(tunit[10],"Flag");

	fits_create_tbl(fptr, BINARY_TBL, nrows, tfields, ttype, tform,
                tunit, extname, &status);
	fits_write_key(fptr, TLONG, "NBIN", &(clus.N_BINARY), 
			"Number of binaries", &status);
	fits_write_key(fptr, TLONG, "NBINW", &(N), 
			"Number of binaries written", &status);
	cmc_fits_printerror(status);

	/* id1 */
	for(i=0;i<N;i++){
		lng_arr[i] = binary[i].id1;
	}
	fits_write_col(fptr, TLONG, 1, firstrow, firstelem, nrows, lng_arr,
                   &status);
	cmc_fits_printerror(status);
	/* id2 */
	for(i=0;i<N;i++){
		lng_arr[i] = binary[i].id2;
	}
	fits_write_col(fptr, TLONG, 2, firstrow, firstelem, nrows, lng_arr,
                   &status);
	cmc_fits_printerror(status);
	/* radius1 */
	for(i=0;i<N;i++){
		dbl_arr[i] =  binary[i].rad1;
	}
	fits_write_col(fptr, TDOUBLE, 3, firstrow, firstelem, nrows, dbl_arr,
                   &status);
	cmc_fits_printerror(status);
	/* radius2 */
	for(i=0;i<N;i++){
		dbl_arr[i] =  binary[i].rad2;
	}
	fits_write_col(fptr, TDOUBLE, 4, firstrow, firstelem, nrows, dbl_arr,
                   &status);
	cmc_fits_printerror(status);
	/* mass1 */
	for(i=0;i<N;i++){
		dbl_arr[i] =  binary[i].m1;
	}
	fits_write_col(fptr, TDOUBLE, 5, firstrow, firstelem, nrows, dbl_arr,
                   &status);
	cmc_fits_printerror(status);
	/* mass2 */
	for(i=0;i<N;i++){
		dbl_arr[i] =  binary[i].m2;
	}
	fits_write_col(fptr, TDOUBLE, 6, firstrow, firstelem, nrows, dbl_arr,
                   &status);
	cmc_fits_printerror(status);
	/* Internal energy 1 */
	for(i=0;i<N;i++){
		dbl_arr[i] =  binary[i].Eint1;
	}
	fits_write_col(fptr, TDOUBLE, 7, firstrow, firstelem, nrows, dbl_arr,
                   &status);
	cmc_fits_printerror(status);
	/* Internal energy 2 */
	for(i=0;i<N;i++){
		dbl_arr[i] =  binary[i].Eint2;
	}
	fits_write_col(fptr, TDOUBLE, 8, firstrow, firstelem, nrows, dbl_arr,
                   &status);
	cmc_fits_printerror(status);
	/* semimajor axis */
	for(i=0;i<N;i++){
		dbl_arr[i] =  binary[i].a;
	}
	fits_write_col(fptr, TDOUBLE, 9, firstrow, firstelem, nrows, dbl_arr,
                   &status);
	cmc_fits_printerror(status);
	/* eccentricity */
	for(i=0;i<N;i++){
		dbl_arr[i] =  binary[i].e;
	}
	fits_write_col(fptr, TDOUBLE, 10, firstrow, firstelem, nrows, dbl_arr,
                   &status);
	cmc_fits_printerror(status);
	/* binary inuse flag */
	for(i=0;i<N;i++){
		int_arr[i] =  binary[i].inuse;
	}
	fits_write_col(fptr, TINT, 11, firstrow, firstelem, nrows, int_arr,
                   &status);
	cmc_fits_printerror(status);


	FREE_TSTUFF
	free(dbl_arr); free(lng_arr); free(int_arr);

}


/* Initially the extensions will be:
 * 0: This is just headers, 
 * 1: Meta information written here
 * 2: Restart parameters, in particular the global variables
 * 3: Single star dynamical parameters
 * 4: Single star stellar evolution parameters
 * 5: Binaries' dynamical parameters
 * 6: Binaries' stellar evolution parameters
 */

void chkpnt_fits(gsl_rng *rng){
	fitsfile *fptr;
	int status;
	static char filename[3][2048];	
				/* we will keep only three files per session */
	static int fileno=0;	/* counter for ensuring only three files per
				 * session are kept to save space. 
				 * these variables aren't kept in snapshot file,
				 * despite being static. this is intentional!
				 *   -- ato 19:25,  5 Apr 2005 (UTC) */

	if(fileno==3){
		/* try removing filename[0] */
		unlink(filename[0]+1); /* +1 is for preceding "!" */
		/* shift filenames */
		strncpy(filename[0], filename[1], 2048);
		strncpy(filename[1], filename[2], 2048);
		/* decrease counter */
		fileno--;
	}
	sprintf(filename[fileno], "!%s_chkpnt%ld.fit", outprefix, tcount);
	status = 0;
	fits_create_file(&fptr, filename[fileno], &status);
	cmc_fits_printerror(status);

	/* 1st Extension, Basic info */
	write_basic_info(fptr);
	/* 2nd Extension: Restart Parameters */
	write_restart_param(fptr, rng);
	/* 3rd Extension: Single Star Dynamical Parameters */
	write_ss_dyn_param(fptr);
	/* 4th Extension: Single Star Stellar Evolution Parameters */
	if (STELLAR_EVOLUTION > 0){
		write_ss_se_param(fptr);
	}
	/* 5th Extension: Binary Star Dynamical Parameters */
	if (clus.N_BINARY > 0) {
		write_bs_dyn_param(fptr);
	}

	fits_close_file(fptr, &status);
	cmc_fits_printerror(status);
	fileno++;
}

#undef ALLOC_TSTUFF 
#undef FREE_TSTUFF
#undef LARGE_DISTANCE
