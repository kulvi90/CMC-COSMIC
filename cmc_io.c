/* -*- linux-c -*- */

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <getopt.h>
#include <gsl/gsl_rng.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "cmc.h"
#include "cmc_vars.h"

/* print the version */
void print_version(FILE *stream)
{
	fprintf(stream, "** %s %s (%s) [%s] **\n", CMCPRETTYNAME, CMCVERSION, CMCNICK, CMCDATE);
}

/* print the usage */
void cmc_print_usage(FILE *stream, char *argv[])
{
	fprintf(stream, "USAGE:\n");
	fprintf(stream, "  %s [options...] <input_file> <output_file_prefix>\n", argv[0]);
	fprintf(stream, "\n");
	fprintf(stream, "OPTIONS:\n");
	fprintf(stream, "  -q --quiet   : do not print diagnostic info to stdout\n");
	fprintf(stream, "  -d --debug   : turn on debugging\n");
	fprintf(stream, "  -V --version : print version info\n");
	fprintf(stream, "  -h --help    : display this help text\n");
}

void print_results(void){
#ifdef USE_MPI
	if(myid==0)
#endif
	{
		PrintLogOutput();
		//PrintFileOutput();
		fflush(NULL);
	}
}

/*********** Output 2D/3D snapshots **************/
void print_2Dsnapshot(void)
{
	long i, j;
	j=0;
	char outfile[100];
	
	if (SNAPSHOTTING) {
		/* open file for 2D snapshot */
		sprintf(outfile, "%s.snap%04ld.dat.gz", outprefix, snap_num);
		if ((snapfile = (FILE *) gzopen(outfile, "wb")) == NULL) {
			eprintf("cannot create 2D snapshot file %s\n", outfile);
			exit_cleanly(1);
		}
		
		/* print useful header */
		gzprintf(snapfile, "# t=%.8g [code units]; All quantities below are in code units unless otherwise specified.\n", TotalTime);
		gzprintf(snapfile, "#1:id #2:m[MSUN] #3:r #4:vr #5:vt #6:E #7:J #8:binflag #9:m0[MSUN] #10:m1[MSUN] #11:id0 #12:id1 #13:a[AU] #14:e #15:startype #16:luminosity[LSUN] #17:radius[RSUN]  #18:bin_startype0 #19:bin_startype1 #20:bin_star_lum0[LSUN] #21:bin_star_lum1[LSUN] #22:bin_star_radius0[RSUN] #23:bin_star_radius1[RSUN] 24.star.phi\n");
		
		/* then print data */
		for (i=1; i<=clus.N_MAX; i++) {
			gzprintf(snapfile, "%ld %.8g %.8g %.8g %.8g %.8g %.8g ", 
				 star[i].id, star[i].m * (units.m / clus.N_STAR) / MSUN, 
				 star[i].r, star[i].vr, star[i].vt, 
				 star[i].E, star[i].J);
			if (star[i].binind) {
				j = star[i].binind;
				gzprintf(snapfile, "1 %.8g %.8g %ld %ld %.8g %.8g ", 
					 binary[j].m1 * (units.m / clus.N_STAR) / MSUN, 
					 binary[j].m2 * (units.m / clus.N_STAR) / MSUN, 
					 binary[j].id1, binary[j].id2,
					 binary[j].a * units.l / AU, binary[j].e);
			} else {
				gzprintf(snapfile, "0 0 0 0 0 0 0 ");	
			}
			
			if (star[i].binind == 0) {
				gzprintf(snapfile, "%d %.8g %.8g -100 -100 -100 -100 -100 -100 ", 
					 star[i].se_k, star[i].se_lum, star[i].rad * units.l / RSUN);
			} else {
				gzprintf(snapfile, "0 0 0 %d %d %.8g %.8g %.8g %.8g ",
					binary[j].bse_kw[0], binary[j].bse_kw[1], 
					binary[j].bse_lum[0], binary[j].bse_lum[1],
					binary[j].rad1*units.l/RSUN, binary[j].rad2*units.l/RSUN);
			}
			gzprintf(snapfile, "%0.12g\n", star[i].phi);
		}
		
		gzclose(snapfile);
		
		/* global counter for snapshot output file */
		snap_num++;
	}
}

void PrintLogOutput(void)
{
	double m, rh, trh, conc_param, m_single, m_binary;
	long ih, k;
	
	/* Computing half-mass radii, and relaxation time */
	m = rh = trh = 0.0;
	rh_binary = rh_single = m_binary = m_single = 0.0;
/*
	for (ih=1; ih<=clus.N_MAX; ih++) {
		k = ih;
#ifdef USE_MPI
		m += star_m[k] / clus.N_STAR;
		
		if (star[k].binind > 0) {
			m_binary += star_m[k] / clus.N_STAR;
		} else {
			m_single += star_m[k] / clus.N_STAR;
		}
		
		if (m/Mtotal <= 0.5) {
			rh = star_r[k];
		}
		if (m_single / (Mtotal - (M_b / clus.N_STAR)) <= 0.5) {
			rh_single = star_r[k];
		}
		// avoid dividing by zero if there are no binaries
		if (M_b > 0) {
			if (m_binary / M_b * clus.N_STAR <= 0.5) {
				rh_binary = star_r[k];
			}
		}
#else
		m += star[k].m / clus.N_STAR;
		
		if (star[k].binind > 0) {
			m_binary += star[k].m / clus.N_STAR;
		} else {
			m_single += star[k].m / clus.N_STAR;
		}
		
		if (m/Mtotal <= 0.5) {
			rh = star[k].r;
		}
		if (m_single / (Mtotal - (M_b / clus.N_STAR)) <= 0.5) {
			rh_single = star[k].r;
		}
		// avoid dividing by zero if there are no binaries
		if (M_b > 0) {
			if (m_binary / M_b * clus.N_STAR <= 0.5) {
				rh_binary = star[k].r;
			}
		}
#endif
	}
*/	

	/* t_rh calculated using r_h */
	trh = ((0.138 * clus.N_MAX) / log((double) clus.N_MAX)) * sqrt((rh * rh * rh) / Mtotal) * log((double) clus.N_MAX) / clus.N_MAX;

	/* Concentration parameter --- note that max_r is the max radius of all bound
	   stars, returned by get_positions(). When a finite R_MAX (i.e. initial Rtidal)
	   is specified, max_r should be approximately equal to Rtidal. But when 
	   Rtidal is very large (isolated cluster), r_max still gives the "actual"
	   size of the cluster, indicating the maximum radius.
	   The conc parameter is actually defined as LOG_10(rtidal/rcore). But for
	   easier reading, it is calculated here only as rtidal/rcore. */
	
	/* max_r & core_radius are output in out_3 file */
	if(max_r == 0.0){
		conc_param = 0.0;
	} else {
		conc_param = (max_r / core_radius);
	}

	gprintf("******************************************************************************\n");
	fprintf(logfile, "******************************************************************************\n");

	gprintf("tcount=%ld TotalTime=%.16e Dt=%.16e\n", tcount, TotalTime, Dt);
	fprintf(logfile, "tcount=%ld TotalTime=%.16e Dt=%.16e\n", tcount, TotalTime, Dt);

	gprintf("Etotal=%g max_r=%g N_bound=%ld Rtidal=%g\n", Etotal.tot, max_r, clus.N_MAX, Rtidal);
	fprintf(logfile, "Etotal=%g max_r=%g N_bound=%ld Rtidal=%g\n", Etotal.tot, max_r, clus.N_MAX, Rtidal);
	
	gprintf("Mtotal=%g Etotal.P=%g Etotal.K=%g VRatio=%g\n", Mtotal, Etotal.P, Etotal.K, -2.0 * Etotal.K / Etotal.P);
	fprintf(logfile, "Mtotal=%g Etotal.P=%g Etotal.K=%g VRatio=%g\n", Mtotal, Etotal.P, Etotal.K, -2.0 * Etotal.K / Etotal.P);
	
	gprintf("TidalMassLoss=%g\n", TidalMassLoss);
	fprintf(logfile, "TidalMassLoss=%g\n", TidalMassLoss);
	
	gprintf("core_radius=%g rho_core=%g v_core=%g Trc=%g conc_param=%g N_core=%g\n",
		core_radius, rho_core, v_core, Trc, conc_param, N_core);
	fprintf(logfile, "core_radius=%g rho_core=%g v_core=%g Trc=%g conc_param=%g N_core=%g\n",
		core_radius, rho_core, v_core, Trc, conc_param, N_core);
	
	gprintf("trh=%g rh=%g rh_single=%g rh_binary=%g\n", trh, rh, rh_single, rh_binary);
	fprintf(logfile, "trh=%g rh=%g rh_single=%g rh_binary=%g\n", trh, rh, rh_single, rh_binary);
	
	gprintf("N_b=%ld M_b=%g E_b=%g\n", N_b, M_b/clus.N_STAR, E_b);
	fprintf(logfile, "N_b=%ld M_b=%g E_b=%g\n", N_b, M_b/clus.N_STAR, E_b);

	gprintf("******************************************************************************\n");
	fprintf(logfile, "******************************************************************************\n");
}

void PrintFileOutput(void) {
	long i, j, n_single, n_binary, n_single_nb, n_binary_nb, N_core_binary, N_core_binary_nb, n_10=1, n_sing_10=0, n_bin_10=0;
	double fb, fb_core, fb_core_nb, m_sing_10=0.0, m_bin_10=0.0, m_10=0.0, r_10=0.0, rho_10=0.0;
	int *multimassr_empty  = (int *) malloc((NO_MASS_BINS-1)*sizeof(int));

	/* print useful headers */
	if (tcount == 1) {
		fprintf(lagradfile, "# Lagrange radii [code units]\n");
		fprintf(ave_mass_file, "# Average mass within Lagrange radii [M_sun]\n");
		fprintf(no_star_file, "# Number of stars within Lagrange radii [dimensionless]\n");
		fprintf(densities_file, "# Density within Lagrange radii [code units]\n");
		fprintf(ke_rad_file, "# Total radial kinetic energy within Lagrange radii [code units]\n");
		fprintf(ke_tan_file, "# Total tangential kinetic energy within Lagrange radii [code units]\n");
		fprintf(v2_rad_file, "# Sum of v_r within Lagrange radii [code units]\n");
		fprintf(v2_tan_file, "# Sum of v_t within Lagrange radii [code units]\n");
		for(i=0; i<NO_MASS_BINS-1; i++){
			fprintf(mlagradfile[i], "# Lagrange radii for %g < m < %g range [code units]\n", mass_bins[i], mass_bins[i+1]);
		}
		
		fprintf(lagradfile, "# 1:t");
		fprintf(ave_mass_file, "# 1:t");
		fprintf(no_star_file, "# 1:t");
		fprintf(densities_file, "# 1:t");
		fprintf(ke_rad_file, "# 1:t");
		fprintf(ke_tan_file, "# 1:t");
		fprintf(v2_rad_file, "# 1:t");
		fprintf(v2_tan_file, "# 1:t");
		for(i=0; i<NO_MASS_BINS-1; i++){
			fprintf(mlagradfile[i], "# 1:t");
		}

		for (i=0; i<MASS_PC_COUNT; i++) {
			fprintf(lagradfile, " %ld:r(%g)", i+2, mass_pc[i]);
			fprintf(ave_mass_file, " %ld:<m>(%g)", i+2, mass_pc[i]);
			fprintf(no_star_file, " %ld:N(%g)", i+2, mass_pc[i]);
			fprintf(densities_file, " %ld:rho(%g)", i+2, mass_pc[i]);
			fprintf(ke_rad_file, " %ld:T_r(%g)", i+2, mass_pc[i]);
			fprintf(ke_tan_file, " %ld:T_t(%g)", i+2, mass_pc[i]);
			fprintf(v2_rad_file, " %ld:V2_r(%g)", i+2, mass_pc[i]);
			fprintf(v2_tan_file, " %ld:V2_t(%g)", i+2, mass_pc[i]);
			for(j=0; j<NO_MASS_BINS-1; j++){
				fprintf(mlagradfile[j], " %ld:r(%g)", i+2, mass_pc[i]);
			}
		}

		fprintf(lagradfile, "\n");
		fprintf(ave_mass_file, "\n");
		fprintf(no_star_file, "\n");
		fprintf(densities_file, "\n");
		fprintf(ke_rad_file, "\n");
		fprintf(ke_tan_file, "\n");
		fprintf(v2_rad_file, "\n");
		fprintf(v2_tan_file, "\n");
		for(i=0; i<NO_MASS_BINS-1; i++){
			fprintf(mlagradfile[i], "\n");
		}
	}

	/* print data */
	fprintf(lagradfile, "%.9e ", TotalTime);
	fprintf(ave_mass_file, "%.9e ",TotalTime);
	fprintf(no_star_file, "%.9e ",TotalTime);
	fprintf(densities_file, "%.9e ",TotalTime);
	fprintf(ke_rad_file, "%.9e ",TotalTime);
	fprintf(ke_tan_file, "%.9e ",TotalTime);
	fprintf(v2_rad_file, "%.9e ",TotalTime);
	fprintf(v2_tan_file, "%.9e ",TotalTime);
	for(i=0; i<NO_MASS_BINS-1; i++){
		multimassr_empty[i] = 1;
		for(j=0; j<NO_MASS_BINS-1; j++){
			if (multi_mass_r[j][i] > 0.0) multimassr_empty[i] = 0;
		}
	}
	for(i=0; i<NO_MASS_BINS-1; i++){
		if ( !multimassr_empty[i] ){
			fprintf(mlagradfile[i], "%.9e ", TotalTime);
		}
	}

	for (i = 0; i < MASS_PC_COUNT ; i++) {
		fprintf(lagradfile, "%e ", mass_r[i]);
		fprintf(ave_mass_file,"%e ", ave_mass_r[i] * units.m / MSUN);
		fprintf(no_star_file,"%g ", no_star_r[i]);
		fprintf(densities_file,"%e ", densities_r[i]);
		fprintf(ke_rad_file,"%e ", ke_rad_r[i]);
		fprintf(ke_tan_file,"%e ", ke_tan_r[i]);
		fprintf(v2_rad_file,"%e ", v2_rad_r[i]);
		fprintf(v2_tan_file,"%e ", v2_tan_r[i]);
		for(j=0; j<NO_MASS_BINS-1; j++){
			if ( !multimassr_empty[j] ){
				fprintf(mlagradfile[j], "%e ", multi_mass_r[j][i]);
			}
		}
	}
	fprintf(lagradfile, "\n");
	fprintf(ave_mass_file,"\n");
	fprintf(no_star_file,"\n");
	fprintf(densities_file,"\n");
	fprintf(ke_rad_file,"\n");
	fprintf(ke_tan_file,"\n");
	fprintf(v2_rad_file,"\n");
	fprintf(v2_tan_file,"\n");
	for(i=0; i<NO_MASS_BINS-1; i++){
		if ( !multimassr_empty[i] ){
			fprintf(mlagradfile[i], "\n");
		}
	}

	/* information on the central BH */
	/* print useful header */
	if (tcount == 1) {
		fprintf(centmass_file, "# Information on central black hole [code units unless otherwise noted]\n");
		fprintf(centmass_file, "#1:t #2:cenma.m #3:Dt #4:rho_core #5:Etotal.tot #6:Etotal.K #7:Etotal.P\n");
	}
	fprintf(centmass_file, "%.9e %.9e %.9e %.9e %.9e %.9e %.9e\n",
		TotalTime, cenma.m * madhoc, Dt, rho_core, Etotal.tot, Etotal.K, Etotal.P);
	
	/* output Time,N_MAX,TotalE,TotalKE,TotalPE,Mtotal */
	/* print useful header */
	if (tcount == 1) {
		fprintf(dynfile, "# Dynamical information [code units]\n");
		fprintf(dynfile, "#1:t #2:Dt #3:tcount #4:N #5:M #6:VR #7:N_c #8:r_c #9:r_max #10:Etot #11:KE #12:PE #13:Etot_int #14:Etot_bin #15:E_cenma #16:Eesc #17:Ebesc #18:Eintesc #19:Eoops #20:Etot+Eoops #21:r_h #22:rho_0 #23:rc_spitzer #24:v0_rms #25:rc_nb #26.DMse(MSUN) #27.DMrejuv(MSUN) #28.N_c_nb\n");
		//Sourav:printing properties at 10 lagrange radii
		if (CALCULATE10){
			fprintf(lagrad10file, "#Dynamical information at 0.1 lagrange radius\n");
			fprintf(lagrad10file, "#1.t #2.Dt #3.tcount #4.N_10 #5.M_10 #6.N_s,10 #7.M_s,10 #8.N_b,10 #9.M_b_10 #10.r_10 #11.rho_10\n");
		}
	}
	fprintf(dynfile, "%.8g %.8g %ld %ld %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g\n",
		TotalTime, Dt, tcount, clus.N_MAX, Mtotal, -2.0*Etotal.K/Etotal.P, N_core, core_radius, max_r, 
		Etotal.tot, Etotal.K, Etotal.P, Etotal.Eint, Etotal.Eb, cenma.E, Eescaped, Ebescaped, Eintescaped, 
		Eoops, Etotal.tot+Eoops, clusdyn.rh, central.rho, central.rc_spitzer, central.v_rms, rc_nb, DMse*units.m/MSUN, DMrejuv*units.m/MSUN, N_core_nb);

	//Sourav: printing properties at 10% lagrange radius
	if (CALCULATE10){
		n_10=1;
		m_bin_10 = 0.0;
		m_sing_10 = 0.0;
		m_10 = 0.0;
		while (m_10 < 0.1 * Mtotal) {
			m_10 += star[n_10].m / clus.N_STAR;
			if (star[n_10].binind>0){
				n_bin_10++;
				m_bin_10 += star[n_10].m / clus.N_STAR;
			}
			else{
				n_sing_10++;
				m_sing_10 += star[n_10].m / clus.N_STAR;
			}
			n_10++;
		}
		/* exit if not enough stars */
		if (n_10 <= 6 || n_10 >= clus.N_STAR-6) {
			eprintf("clus.N_STAR <= 2*J || n_10 >= clus.N_STAR-6\n");
			exit_cleanly(-1);
		}
		else{
			r_10=star[n_10].r;
			rho_10 = m_10/(4.0 * 3.0 * PI * fb_cub(r_10));	
		}
		fprintf(lagrad10file, "%.8g %.8g %.ld %ld %.8g %ld %.8g %ld %.8g %.8g %.8g\n",
					TotalTime, Dt, tcount, n_10, m_10, n_sing_10, m_sing_10, n_bin_10, m_bin_10, r_10, rho_10);
	}

	/* Output binary data Note: N_BINARY counts ALL binaries (including escaped/destroyed ones)
	   whereas N_b only counts EXISTING BOUND binaries. */
	/* calculate core binary fraction */
	n_single = 0;
	n_binary = 0;
	//Sourav:initialize nb core properties
	n_single_nb = 0;
	n_binary_nb = 0;
	for (i=1; star[i].r<=core_radius; i++) {
		if (star[i].binind > 0) {
			n_binary++;
		} else {
			n_single++;
		}
	}
	N_core_binary = n_binary;
	
	//Sourav:calculate n_sing and n_bin for nb core
	for (i=1; star[i].r<=rc_nb; i++) {
		if (star[i].binind > 0) {
			n_binary_nb++;
		} else {
			n_single_nb++;
		}
	}
	N_core_binary_nb = n_binary_nb;
	
	/* this is such a kludge: core_radius is not initialized on the first timestep */
	if (n_single + n_binary == 0) {
		fb_core = 0.0;
	} else {
		fb_core = ((double) n_binary)/((double) (n_single + n_binary));
	}
	
	//calculate the same for nb core
	if (n_single_nb + n_binary_nb == 0) {
		fb_core_nb = 0.0;
	} else {
		fb_core_nb = ((double) n_binary_nb)/((double) (n_single_nb + n_binary_nb));
	}
	
	/* calculate overall binary fraction */
	n_single = 0;
	n_binary = 0;
	for (i=1; i<=clus.N_MAX; i++) {
		if (star[i].binind > 0) {
			n_binary++;
		} else {
			n_single++;
		}
	}
	/* this is such a kludge: core_radius is not initialized on the first timestep */
	if (n_single + n_binary == 0) {
		fb = 0.0;
	} else {
		fb = ((double) n_binary)/((double) (n_single + n_binary));
	}
	
	/* print useful header */
	if (tcount == 1) {
		fprintf(binaryfile, "# Binary information [code units]\n");
		fprintf(binaryfile, "# 1:t 2:N_b 3:M_b 4:E_b 5:r_h,s 6:r_h,b 7:rho_c,s 8:rho_c,b 9:N_bb 10:N_bs 11:f_b,c 12:f_b 13:E_bb 14:E_bs 15:DE_bb 16:DE_bs 17:N_bc,nb 18:f_b,c,nb 19:N_bc \n");
	}
	/* print data */
	fprintf(binaryfile,
		"%.6g %ld %.6g %.6g %.6g %.6g %.6g %.6g %ld %ld %.6g %.6g %.6g %.6g %.6g %.6g %ld %.8g %ld\n",
		TotalTime, N_b, M_b, E_b, rh_single, 
		rh_binary, rho_core_single, rho_core_bin, 
		N_bb, N_bs, fb_core, 
		fb, E_bb, E_bs, DE_bb, DE_bs, 
		N_core_binary_nb, fb_core_nb, N_core_binary);

        if (WRITE_EXTRA_CORE_INFO) {
          write_core_data(corefile, no_remnants);
          fprintf(corefile, "\n");
        }

	/* also saves INITIAL snapshot (StepCount=0) */
	if (TotalTime >= SNAPSHOT_DELTAT * StepCount) {
		StepCount++;
		print_2Dsnapshot();
		if (WRITE_STELLAR_INFO){
			write_stellar_data();	
		}
	}

	print_snapshot_windows();
	free(multimassr_empty);
}

/*** Parsing of Input Parameters / Memory allocation / File I/O ***/
void parser(int argc, char *argv[], gsl_rng *r)
{
	char inputfile[1024], outfile[1024], outfilemode[5];
	char parameter_name[1024], values[1024], dummy[1024], line[2048];
	char *curr_mass;
	parsed_t parsed;
	parsed_t *spp;
	long i;
	int allparsed=1;
	/* int *ip; */
	FILE *in, *parsedfp;
	const char *short_opts = "qdVhs:";
	const struct option long_opts[] = {
		{"quiet", no_argument, NULL, 'q'},
		{"debug", no_argument, NULL, 'd'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{"streams", required_argument, NULL, 's'}, //Run with multiple random streams. To mimic the parallel version with the given number of processors
		{NULL, 0, NULL, 0}
	};
	
	/* DEFAULT PARAMETER VALUES */
	quiet = 0;
	debug = 0;
	NO_MASS_BINS = 0;
	snapshot_window_count=0;
	/* DEFAULT PARAMETER VALUES */
	
	while ((i = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (i) {
		case 'q':
			quiet = 1;
			break;
		case 'd':
			debug = 1;
			break;
		case 'V':
			print_version(stdout);
			exit(0);
		case 's':
			procs = atoi(optarg);
			break;
		case 'h':
			print_version(stdout);
			fprintf(stdout, "\n");
			cmc_print_usage(stdout, argv);
			exit(0);
		default:
			break;
		}
	}

	/* check to make sure there was nothing crazy on the command line */
	if (argc - optind != 2) {
		cmc_print_usage(stdout, argv);
		exit(0);
	}

	/* set inputfile and outprefix now that the options have been parsed */
	sprintf(inputfile, "%s", argv[optind]);
	sprintf(outprefix, "%s", argv[optind+1]);
#ifdef USE_MPI
	strcpy(outprefix_bak, outprefix);
	sprintf(outprefix, "%s%d", outprefix, myid);
#endif
	dprintf("inputfile=%s outprefix=%s\n", inputfile, outprefix);

	/*======= Opening of input & output files =======*/
	if ((in = fopen(inputfile, "r")) == NULL) {
		eprintf("Cannot open input file \"%s\".\n", inputfile);
		exit(1);
	}
	
	sprintf(outfile, "%s.cmc.parsed", outprefix);
	if ((parsedfp = fopen(outfile, "w")) == NULL) {
		eprintf("cannot create output file \"%s\".\n", outfile);
		exit(1);
	}

	/* nothing is set yet, so assigning all zeros to variable parsed */
	/* one way to do it is a complicated for loop (?which depends on the 
	 * fact that all members of the struct is int?), this is the fun way */
	//ip = (int *) &parsed;
	//for(i=0; i<(sizeof(parsed)/sizeof(int)); i++){
	//	*(ip++) = 0;
	//}
	/* faster and portable way to do this is using calloc, this does
	 * not depend on the structure of the struct */
	spp = (parsed_t *) calloc(1, sizeof(parsed_t));
	parsed = *spp;
	free(spp);
	
	while (fgets(line, 2048, in)) {
		/* the maximum size of an input line is limited by 
		 * the size of the line array */
		if (strlen(line) == 2047) {
			eprintf("input line too long: \"%s\".\n", line);
			exit(1);
		}
		/* strip comments and newline character */
		line[strcspn(line, "#\n")] = '\0';
		
		/* replace equal sign with a space */
		while (strchr(line, '=') != NULL) {
			line[strcspn(line, "=")] = ' ';
		}
		
		/* convert lowercase letters to uppercase in the keyword only */
		i = 0;
		while (!(isspace(line[i]))) {
			line[i] = toupper(line[i]);
			i++;
		}

#define PRINT_PARSED(DOC) fprintf(parsedfp, "# %s\n%s\n", DOC, line)

		/* see if there are too many values for parameter */
		if (sscanf(line, "%s %s %s", parameter_name, values, dummy) == 3) {
			eprintf("too many values for parameter: \"%s\".\n", line);
			exit(1);
		} else if (sscanf(line, "%s %s", parameter_name, values) == 2) {
			if (strcmp(parameter_name, "BINBIN") == 0) {
				PRINT_PARSED(PARAMDOC_BINBIN);
				sscanf(values, "%d", &BINBIN);
				parsed.BINBIN = 1;
			} else if (strcmp(parameter_name, "BINSINGLE") == 0) {
				PRINT_PARSED(PARAMDOC_BINSINGLE);
				sscanf(values, "%d", &BINSINGLE);
				parsed.BINSINGLE = 1;
			} else if (strcmp(parameter_name, "STREAMS") == 0) {
				PRINT_PARSED(PARAMDOC_STREAMS);
				sscanf(values, "%d", &procs);
				parsed.STREAMS = 1;
			} else if (strcmp(parameter_name, "SNAPSHOTTING") == 0) {
				PRINT_PARSED(PARAMDOC_SNAPSHOTTING);
				sscanf(values, "%d", &SNAPSHOTTING);
				parsed.SNAPSHOTTING = 1;
			} else if (strcmp(parameter_name, "SNAPSHOT_DELTACOUNT") == 0) {
				PRINT_PARSED(PARAMDOC_SNAPSHOT_DELTACOUNT);
				sscanf(values, "%ld", &SNAPSHOT_DELTACOUNT);
				parsed.SNAPSHOT_DELTACOUNT = 1;
			} else if (strcmp(parameter_name, "SNAPSHOT_DELTAT") == 0) {
				PRINT_PARSED(PARAMDOC_SNAPSHOT_DELTAT);
				sscanf(values, "%lf", &SNAPSHOT_DELTAT);
				parsed.SNAPSHOT_DELTAT = 1;
			} else if (strcmp(parameter_name, "SNAPSHOT_CORE_BOUNCE") == 0) {
				PRINT_PARSED(PARAMDOC_SNAPSHOT_CORE_BOUNCE);
				sscanf(values, "%d", &SNAPSHOT_CORE_BOUNCE);
				parsed.SNAPSHOT_CORE_BOUNCE = 1;
			} else if (strcmp(parameter_name, "SNAPSHOT_CORE_COLLAPSE") == 0) {
				PRINT_PARSED(PARAMDOC_SNAPSHOT_CORE_COLLAPSE);
				sscanf(values, "%d", &SNAPSHOT_CORE_COLLAPSE);
				parsed.SNAPSHOT_CORE_COLLAPSE = 1;
			} else if (strcmp(parameter_name, "SNAPSHOT_WINDOWS") == 0) {
				PRINT_PARSED(PARAMDOC_SNAPSHOT_WINDOWS);
				SNAPSHOT_WINDOWS= (char *) malloc(sizeof(char)*300);
				parse_snapshot_windows(values);
				parsed.SNAPSHOT_WINDOWS = 1;
			} else if (strcmp(parameter_name, "SNAPSHOT_WINDOW_UNITS") == 0) {
				PRINT_PARSED(PARAMDOC_SNAPSHOT_WINDOW_UNITS);
				SNAPSHOT_WINDOW_UNITS= (char *) malloc(sizeof(char)*10);
				strncpy(SNAPSHOT_WINDOW_UNITS,values, 10);
				if (!valid_snapshot_window_units()) {
					eprintf("Unrecognized snapshot window time unit %s.", values);
					free_arrays();
					exit(-1);
				}
				parsed.SNAPSHOT_WINDOW_UNITS = 1;
			} else if (strcmp(parameter_name, "IDUM") == 0) {
				PRINT_PARSED(PARAMDOC_IDUM);
				sscanf(values, "%ld", &IDUM);
				parsed.IDUM = 1;
			} else if (strcmp(parameter_name, "INPUT_FILE") == 0) {
				PRINT_PARSED(PARAMDOC_INPUT_FILE);
				sscanf(values, "%s", INPUT_FILE);
				parsed.INPUT_FILE = 1;
			} else if (strcmp(parameter_name, "MASS_PC_BH_INCLUDE") == 0) {
				PRINT_PARSED(PARAMDOC_MASS_PC_BH_INCLUDE);
				sscanf(values, "%d", &MASS_PC_BH_INCLUDE);
				parsed.MASS_PC_BH_INCLUDE = 1;
			} else if (strcmp(parameter_name, "MASS_PC") == 0) {
				PRINT_PARSED(PARAMDOC_MASS_PC);
				strcpy(MASS_PC, values);
				curr_mass = (char *) strtok(values, ",; ");
				for (MASS_PC_COUNT = 1; (curr_mass = (char *) strtok(NULL, " ,;")) != NULL; MASS_PC_COUNT++);
				parsed.MASS_PC = 1;
			} else if (strcmp(parameter_name, "MASS_BINS") == 0) {
				PRINT_PARSED(PARAMDOC_MASS_BINS);
				/* we recycle variable "curr_mass" for mass bins */
				strcpy(MASS_BINS, values);
				curr_mass = (char *) strtok(values, ",; ");
				for (NO_MASS_BINS = 1; (curr_mass = (char *) strtok(NULL, " ,;")) != NULL; NO_MASS_BINS++);
				parsed.MASS_BINS = 1;
			} else if (strcmp(parameter_name, "MINIMUM_R") == 0) {
				PRINT_PARSED(PARAMDOC_MINIMUM_R);
				sscanf(values, "%lf", &MINIMUM_R);
				parsed.MINIMUM_R = 1;
			} else if (strcmp(parameter_name, "STOPATCORECOLLAPSE") == 0) {
				PRINT_PARSED(PARAMDOC_STOPATCORECOLLAPSE);
				sscanf(values, "%d", &STOPATCORECOLLAPSE);
				parsed.STOPATCORECOLLAPSE = 1;
			} else if (strcmp(parameter_name, "NUM_CENTRAL_STARS") == 0) {
				PRINT_PARSED(PARAMDOC_NUM_CENTRAL_STARS);
				sscanf(values, "%ld", &NUM_CENTRAL_STARS);
				parsed.NUM_CENTRAL_STARS = 1;
			} else if (strcmp(parameter_name, "PERTURB") == 0) {
				PRINT_PARSED(PARAMDOC_PERTURB);
				sscanf(values, "%ld", &PERTURB);
				parsed.PERTURB = 1;
			} else if (strcmp(parameter_name, "RELAXATION") == 0) {
				PRINT_PARSED(PARAMDOC_RELAXATION);
				sscanf(values, "%ld", &RELAXATION);
				parsed.RELAXATION = 1;
			} else if (strcmp(parameter_name, "THETASEMAX") == 0) {
				PRINT_PARSED(PARAMDOC_THETASEMAX);
				sscanf(values, "%lf", &THETASEMAX);
				parsed.THETASEMAX = 1;
			} else if (strcmp(parameter_name, "STELLAR_EVOLUTION") == 0) {
				PRINT_PARSED(PARAMDOC_STELLAR_EVOLUTION);
				sscanf(values, "%ld", &STELLAR_EVOLUTION);
				parsed.STELLAR_EVOLUTION = 1;
			} else if (strcmp(parameter_name, "TIDAL_TREATMENT") == 0) {
				PRINT_PARSED(PARAMDOC_TIDAL_TREATMENT);
				sscanf(values, "%ld", &TIDAL_TREATMENT);
				parsed.TIDAL_TREATMENT = 1;
			} else if (strcmp(parameter_name, "SS_COLLISION") == 0) {
				PRINT_PARSED(PARAMDOC_SS_COLLISION);
				sscanf(values, "%ld", &SS_COLLISION);
				parsed.SS_COLLISION = 1;
			} else if (strcmp(parameter_name, "TIDAL_CAPTURE") == 0) {
				PRINT_PARSED(PARAMDOC_TIDAL_CAPTURE);
				sscanf(values, "%ld", &TIDAL_CAPTURE);
				parsed.TIDAL_CAPTURE = 1;
			} /*Sourav:new parameter*/
			else if (strcmp(parameter_name, "STAR_AGING_SCHEME") == 0) {
			 	PRINT_PARSED(PARAMDOC_STAR_AGING_SCHEME);
				sscanf(values, "%ld", &STAR_AGING_SCHEME);
				parsed.STAR_AGING_SCHEME = 1;
			} else if  (strcmp(parameter_name, "PREAGING") == 0) {
			 	PRINT_PARSED(PARAMDOC_PREAGING);
				sscanf(values, "%ld", &PREAGING);
				parsed.PREAGING = 1;
			} else if (strcmp(parameter_name, "TERMINAL_ENERGY_DISPLACEMENT") == 0) {
				PRINT_PARSED(PARAMDOC_TERMINAL_ENERGY_DISPLACEMENT);
				sscanf(values, "%lf", &TERMINAL_ENERGY_DISPLACEMENT);
				parsed.TERMINAL_ENERGY_DISPLACEMENT = 1;
			} else if (strcmp(parameter_name, "T_MAX") == 0) {
				PRINT_PARSED(PARAMDOC_T_MAX);
				sscanf(values, "%lf", &T_MAX);
				parsed.T_MAX = 1;
			} else if (strcmp(parameter_name, "T_MAX_PHYS") == 0) {
				PRINT_PARSED(PARAMDOC_T_MAX_PHYS);
				sscanf(values, "%lf", &T_MAX_PHYS);
				parsed.T_MAX_PHYS = 1;
			} else if (strcmp(parameter_name, "T_MAX_COUNT") == 0) {
				PRINT_PARSED(PARAMDOC_T_MAX_COUNT);
				sscanf(values, "%ld", &T_MAX_COUNT);
				parsed.T_MAX_COUNT = 1;
			} else if (strcmp(parameter_name, "MAX_WCLOCK_TIME") == 0) {
				PRINT_PARSED(PARAMDOC_MAX_WCLOCK_TIME);
				sscanf(values, "%ld", &MAX_WCLOCK_TIME);
				parsed.MAX_WCLOCK_TIME = 1;
			} else if (strcmp(parameter_name, "WIND_FACTOR") == 0) {
				PRINT_PARSED(PARAMDOC_WIND_FACTOR);
				sscanf(values, "%lf", &WIND_FACTOR);
				parsed.WIND_FACTOR = 1;
			} else if (strcmp(parameter_name, "GAMMA") == 0) {
				PRINT_PARSED(PARAMDOC_GAMMA);
				sscanf(values, "%lf", &GAMMA);
				parsed.GAMMA = 1;
			} else if (strcmp(parameter_name, "SEARCH_GRID")== 0) {
				PRINT_PARSED(PARAMDOC_SEARCH_GRID);
				sscanf(values, "%ld", &SEARCH_GRID);
				parsed.SEARCH_GRID = 1;
			} else if (strcmp(parameter_name, "SG_STARSPERBIN")== 0) {
				PRINT_PARSED(PARAMDOC_SG_STARSPERBIN);
				sscanf(values, "%ld", &SG_STARSPERBIN);
				parsed.SG_STARSPERBIN = 1;
			} else if (strcmp(parameter_name, "SG_MAXLENGTH")== 0) {
				PRINT_PARSED(PARAMDOC_SG_MAXLENGTH);
				sscanf(values, "%ld", &SG_MAXLENGTH);
				parsed.SG_MAXLENGTH = 1;
			} else if (strcmp(parameter_name, "SG_MINLENGTH")== 0) {
				PRINT_PARSED(PARAMDOC_SG_MINLENGTH);
				sscanf(values, "%ld", &SG_MINLENGTH);
				parsed.SG_MINLENGTH = 1;
			} else if (strcmp(parameter_name, "SG_POWER_LAW_EXPONENT")== 0) {
				PRINT_PARSED(PARAMDOC_SG_POWER_LAW_EXPONENT);
				sscanf(values, "%lf", &SG_POWER_LAW_EXPONENT);
				parsed.SG_POWER_LAW_EXPONENT = 1;
			} else if (strcmp(parameter_name, "SG_MATCH_AT_FRACTION")== 0) {
				PRINT_PARSED(PARAMDOC_SG_MATCH_AT_FRACTION);
				sscanf(values, "%lf", &SG_MATCH_AT_FRACTION);
				parsed.SG_MATCH_AT_FRACTION = 1;
			} else if (strcmp(parameter_name, "SG_PARTICLE_FRACTION")== 0) {
				PRINT_PARSED(PARAMDOC_SG_PARTICLE_FRACTION);
				sscanf(values, "%lf", &SG_PARTICLE_FRACTION);
				parsed.SG_PARTICLE_FRACTION = 1;
			} else if (strcmp(parameter_name, "BH_LOSS_CONE")== 0) {
				PRINT_PARSED(PARAMDOC_BH_LOSS_CONE);
				sscanf(values, "%li", &BH_LOSS_CONE);
				parsed.BH_LOSS_CONE = 1;
			} else if (strcmp(parameter_name, "BH_R_DISRUPT_NB")== 0) {
				PRINT_PARSED(PARAMDOC_BH_R_DISRUPT_NB);
				sscanf(values, "%lf", &BH_R_DISRUPT_NB);
				parsed.BH_R_DISRUPT_NB = 1;
			} else if (strcmp(parameter_name, "FORCE_RLX_STEP")== 0) {
				PRINT_PARSED(PARAMDOC_FORCE_RLX_STEP);
				sscanf(values, "%i", &FORCE_RLX_STEP);
				parsed.FORCE_RLX_STEP = 1;
        		} else if (strcmp(parameter_name, "DT_HARD_BINARIES")== 0) {
				PRINT_PARSED(PARAMDOC_DT_HARD_BINARIES);
				sscanf(values, "%i", &DT_HARD_BINARIES);
				parsed.DT_HARD_BINARIES = 1;
        		} else if (strcmp(parameter_name, "HARD_BINARY_KT")== 0) {
				PRINT_PARSED(PARAMDOC_HARD_BINARY_KT);
				sscanf(values, "%lf", &HARD_BINARY_KT);
				parsed.HARD_BINARY_KT = 1;
#ifdef EXPERIMENTAL
			} else if (strcmp(parameter_name, "BH_LC_FDT")== 0) {
				PRINT_PARSED(PARAMDOC_BH_LC_FDT);
				sscanf(values, "%lf", &BH_LC_FDT);
				parsed.BH_LC_FDT = 1;
			} else if (strcmp(parameter_name, "AVEKERNEL")== 0) {
				PRINT_PARSED(PARAMDOC_AVEKERNEL);
				sscanf(values, "%li", &AVEKERNEL);
				parsed.AVEKERNEL = 1;
#endif
			} else if (strcmp(parameter_name, "APSIDES_PRECISION")== 0) {
				PRINT_PARSED(PARAMDOC_APSIDES_PRECISION);
				sscanf(values, "%lf", &APSIDES_PRECISION);
				parsed.APSIDES_PRECISION = 1;
			} else if (strcmp(parameter_name, "APSIDES_MAX_ITER")== 0) {
				PRINT_PARSED(PARAMDOC_APSIDES_MAX_ITER);
				sscanf(values, "%li", &APSIDES_MAX_ITER);
				parsed.APSIDES_MAX_ITER = 1;
			} else if (strcmp(parameter_name, "APSIDES_CONVERGENCE")== 0) {
				PRINT_PARSED(PARAMDOC_APSIDES_CONVERGENCE);
				sscanf(values, "%lf", &APSIDES_CONVERGENCE);
				parsed.APSIDES_CONVERGENCE = 1;
			} else if (strcmp(parameter_name, "CIRC_PERIOD_THRESHOLD")== 0) {
				PRINT_PARSED(PARAMDOC_CIRC_PERIOD_THRESHOLD);
				sscanf(values, "%lf", &CIRC_PERIOD_THRESHOLD);
				parsed.CIRC_PERIOD_THRESHOLD = 1;
			} else if (strcmp(parameter_name, "WRITE_STELLAR_INFO")== 0) {
				PRINT_PARSED(PARAMDOC_WRITE_STELLAR_INFO);
				sscanf(values, "%i", &WRITE_STELLAR_INFO);
				parsed.WRITE_STELLAR_INFO = 1;
			} else if (strcmp(parameter_name, "WRITE_RWALK_INFO")== 0) {
				PRINT_PARSED(PARAMDOC_WRITE_RWALK_INFO);
				sscanf(values, "%i", &WRITE_RWALK_INFO);
				parsed.WRITE_RWALK_INFO = 1;
			} else if (strcmp(parameter_name, "WRITE_EXTRA_CORE_INFO")== 0) {
				PRINT_PARSED(PARAMDOC_WRITE_EXTRA_CORE_INFO);
				sscanf(values, "%i", &WRITE_EXTRA_CORE_INFO);
				parsed.WRITE_EXTRA_CORE_INFO = 1;
			} else if (strcmp(parameter_name, "CALCULATE10")== 0) {
				PRINT_PARSED(PARAMDOC_CALCULATE10);
				sscanf(values, "%i", &CALCULATE10);
				parsed.CALCULATE10 = 1;
			} else if (strcmp(parameter_name, "OVERWRITE_RVIR")== 0) {
				PRINT_PARSED(PARAMDOC_OVERWRITE_RVIR);
				sscanf(values, "%lf", &OVERWRITE_RVIR);
				parsed.OVERWRITE_RVIR = 1;
			} else if (strcmp(parameter_name, "OVERWRITE_Z")== 0) {
				PRINT_PARSED(PARAMDOC_OVERWRITE_Z);
				sscanf(values, "%lf", &OVERWRITE_Z);
				parsed.OVERWRITE_Z = 1;
			} else if (strcmp(parameter_name, "OVERWRITE_RTID")== 0) {
				PRINT_PARSED(PARAMDOC_OVERWRITE_RTID);
				sscanf(values, "%lf", &OVERWRITE_RTID);
				parsed.OVERWRITE_RTID = 1;
			} else if (strcmp(parameter_name, "OVERWRITE_MCLUS")== 0) {
				PRINT_PARSED(PARAMDOC_OVERWRITE_MCLUS);
				sscanf(values, "%lf", &OVERWRITE_MCLUS);
				parsed.OVERWRITE_MCLUS = 1;
				// Begin reading in stellar and binary evolution assumptions.
			} else if (strcmp(parameter_name, "BSE_NETA")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_NETA);
				sscanf(values, "%lf", &BSE_NETA);
				parsed.BSE_NETA = 1;
			} else if (strcmp(parameter_name, "BSE_BWIND")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_BWIND);
				sscanf(values, "%lf", &BSE_BWIND);
				parsed.BSE_BWIND = 1;
			} else if (strcmp(parameter_name, "BSE_HEWIND")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_HEWIND);
				sscanf(values, "%lf", &BSE_HEWIND);
				parsed.BSE_HEWIND = 1;
			} else if (strcmp(parameter_name, "BSE_WINDFLAG")==0) {
				PRINT_PARSED(PARAMDOC_BSE_WINDFLAG);
				sscanf(values, "%lf", &BSE_WINDFLAG);
				parsed.BSE_WINDFLAG = 1;
			} else if (strcmp(parameter_name, "BSE_ALPHA1")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_ALPHA1);
				sscanf(values, "%lf", &BSE_ALPHA1);
				parsed.BSE_ALPHA1 = 1;
			} else if (strcmp(parameter_name, "BSE_LAMBDA")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_LAMBDA);
				sscanf(values, "%lf", &BSE_LAMBDA);
				parsed.BSE_LAMBDA = 1;
			} else if (strcmp(parameter_name, "BSE_CEFLAG")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_CEFLAG);
				sscanf(values, "%i", &BSE_CEFLAG);
				parsed.BSE_CEFLAG = 1;
			} else if (strcmp(parameter_name, "BSE_TFLAG")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_TFLAG);
				sscanf(values, "%i", &BSE_TFLAG);
				parsed.BSE_TFLAG = 1;
			} else if (strcmp(parameter_name, "BSE_IFFLAG")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_IFFLAG);
				sscanf(values, "%i", &BSE_IFFLAG);
				parsed.BSE_IFFLAG = 1;
			} else if (strcmp(parameter_name, "BSE_WDFLAG")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_WDFLAG);
				sscanf(values, "%i", &BSE_WDFLAG);
				parsed.BSE_WDFLAG = 1;
			} else if (strcmp(parameter_name, "BSE_BHFLAG")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_BHFLAG);
				sscanf(values, "%i", &BSE_BHFLAG);
				parsed.BSE_BHFLAG = 1;
			} else if (strcmp(parameter_name, "BSE_NSFLAG")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_NSFLAG);
				sscanf(values, "%i", &BSE_NSFLAG);
				parsed.BSE_NSFLAG = 1;
			} else if (strcmp(parameter_name, "BSE_MXNS")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_MXNS);
				sscanf(values, "%lf", &BSE_MXNS);
				parsed.BSE_MXNS = 1;
			} else if (strcmp(parameter_name, "BSE_BCONST")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_BCONST);
				sscanf(values, "%lf", &BSE_BCONST);
				parsed.BSE_BCONST = 1;
			} else if (strcmp(parameter_name, "BSE_CK")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_CK);
				sscanf(values, "%lf", &BSE_CK);
				parsed.BSE_CK = 1;
			} else if (strcmp(parameter_name, "BSE_IDUM")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_IDUM);
				sscanf(values, "%i", &BSE_IDUM);
				parsed.BSE_IDUM = 1;
			} else if (strcmp(parameter_name, "BSE_BETA")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_BETA);
				sscanf(values, "%lf", &BSE_BETA);
				parsed.BSE_BETA = 1;
			} else if (strcmp(parameter_name, "BSE_SIGMA")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_SIGMA);
				sscanf(values, "%lf", &BSE_SIGMA);
				parsed.BSE_SIGMA = 1;
			} else if (strcmp(parameter_name, "BSE_EDDFAC")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_EDDFAC);
				sscanf(values, "%lf", &BSE_EDDFAC);
				parsed.BSE_EDDFAC = 1;
			} else if (strcmp(parameter_name, "BSE_GAMMA")== 0) {
				PRINT_PARSED(PARAMDOC_BSE_GAMMA);
				sscanf(values, "%lf", &BSE_GAMMA);
				parsed.BSE_GAMMA = 1;
			} else {
				wprintf("unknown parameter: \"%s\".\n", line);
			}
		} else if (sscanf(line, "%s", parameter_name) == 1) {
			eprintf("too few values for parameter: \"%s\".\n", line);
			exit(1);
		}
	}
	fclose(in);
	
	/* quit if some parameters are unset */
#define CHECK_PARSED(A) \
	if (parsed.A == 0) { \
		eprintf("parameter \"%s\" unset.\n", #A); \
		allparsed = 0; \
	}
	
	CHECK_PARSED(GAMMA);
	CHECK_PARSED(INPUT_FILE);
	CHECK_PARSED(MASS_PC);
	CHECK_PARSED(MASS_BINS);
	
#undef CHECK_PARSED

/* but only warn if some other parameters are unset and default values are used */
#define CHECK_PARSED(A,DEFAULT,DOC) \
	if (parsed.A == 0) { \
		wprintf("parameter \"%s\" unset: using default value \"%s\").\n", #A, #DEFAULT); \
                A=DEFAULT; \
                fprintf(parsedfp, "# %s\n%s %s     # default value\n", DOC, #A, #DEFAULT); \
	}
	
	CHECK_PARSED(MASS_PC_BH_INCLUDE, 1, PARAMDOC_MASS_PC_BH_INCLUDE);
	CHECK_PARSED(PERTURB, 1, PARAMDOC_PERTURB);
	CHECK_PARSED(RELAXATION, 1, PARAMDOC_RELAXATION);
	CHECK_PARSED(THETASEMAX, 1.0, PARAMDOC_THETASEMAX);
	CHECK_PARSED(STELLAR_EVOLUTION, 0, PARAMDOC_STELLAR_EVOLUTION);
	CHECK_PARSED(WRITE_STELLAR_INFO, 0, PARAMDOC_WRITE_STELLAR_INFO);
	CHECK_PARSED(WRITE_RWALK_INFO, 0, PARAMDOC_WRITE_RWALK_INFO);
	CHECK_PARSED(WRITE_EXTRA_CORE_INFO, 0, PARAMDOC_WRITE_EXTRA_CORE_INFO);
	CHECK_PARSED(CALCULATE10, 0, PARAMDOC_CALCULATE10);
	CHECK_PARSED(WIND_FACTOR, 1.0, PARAMDOC_WIND_FACTOR);
	CHECK_PARSED(TIDAL_TREATMENT, 0, PARAMDOC_TIDAL_TREATMENT);
	CHECK_PARSED(SS_COLLISION, 0, PARAMDOC_SS_COLLISION);
	CHECK_PARSED(TIDAL_CAPTURE, 0, PARAMDOC_TIDAL_CAPTURE);
	/*Sourav: new parameter*/
	CHECK_PARSED(STAR_AGING_SCHEME, 0, PARAMDOC_STAR_AGING_SCHEME);
	CHECK_PARSED(PREAGING, 0, PARAMDOC_PREAGING);
	CHECK_PARSED(BINBIN, 1, PARAMDOC_BINBIN);
	CHECK_PARSED(BINSINGLE, 1, PARAMDOC_BINSINGLE);
	CHECK_PARSED(STREAMS, 1, PARAMDOC_STREAMS);
	CHECK_PARSED(BH_LOSS_CONE, 0, PARAMDOC_BH_LOSS_CONE);
	CHECK_PARSED(MINIMUM_R, 0.0, PARAMDOC_MINIMUM_R);
	CHECK_PARSED(BH_R_DISRUPT_NB, 0., PARAMDOC_BH_R_DISRUPT_NB);
	CHECK_PARSED(CIRC_PERIOD_THRESHOLD, 1e-18, PARAMDOC_CIRC_PERIOD_THRESHOLD);

	CHECK_PARSED(T_MAX, 20.0, PARAMDOC_T_MAX);
	CHECK_PARSED(T_MAX_PHYS, 12.0, PARAMDOC_T_MAX_PHYS);
	CHECK_PARSED(T_MAX_COUNT, 1000000, PARAMDOC_T_MAX_COUNT);
	CHECK_PARSED(MAX_WCLOCK_TIME, 2592000, PARAMDOC_MAX_WCLOCK_TIME);
	CHECK_PARSED(STOPATCORECOLLAPSE, 1, PARAMDOC_STOPATCORECOLLAPSE);
	CHECK_PARSED(TERMINAL_ENERGY_DISPLACEMENT, 0.5, PARAMDOC_TERMINAL_ENERGY_DISPLACEMENT);

	CHECK_PARSED(SNAPSHOTTING, 0, PARAMDOC_SNAPSHOTTING);
	CHECK_PARSED(SNAPSHOT_DELTACOUNT, 250, PARAMDOC_SNAPSHOT_DELTACOUNT);
	CHECK_PARSED(SNAPSHOT_DELTAT, 0.25, PARAMDOC_SNAPSHOT_DELTAT);
	CHECK_PARSED(SNAPSHOT_CORE_COLLAPSE, 0, PARAMDOC_SNAPSHOT_CORE_COLLAPSE);
	CHECK_PARSED(SNAPSHOT_CORE_BOUNCE, 0, PARAMDOC_SNAPSHOT_CORE_BOUNCE);
	CHECK_PARSED(SNAPSHOT_WINDOWS, NULL, PARAMDOC_SNAPSHOT_WINDOWS);
	CHECK_PARSED(SNAPSHOT_WINDOW_UNITS, "Trel", PARAMDOC_SNAPSHOT_WINDOW_UNITS);

	CHECK_PARSED(NUM_CENTRAL_STARS, 300, PARAMDOC_NUM_CENTRAL_STARS);
	CHECK_PARSED(IDUM, 0, PARAMDOC_IDUM);

	CHECK_PARSED(SEARCH_GRID, 0, PARAMDOC_SEARCH_GRID);
	CHECK_PARSED(SG_STARSPERBIN, 100, PARAMDOC_SG_STARSPERBIN);
	CHECK_PARSED(SG_MAXLENGTH, 1000000, PARAMDOC_SG_MAXLENGTH);
	CHECK_PARSED(SG_MINLENGTH, 1000, PARAMDOC_SG_MINLENGTH);
	CHECK_PARSED(SG_POWER_LAW_EXPONENT, 0.5, PARAMDOC_SG_POWER_LAW_EXPONENT);
	CHECK_PARSED(SG_MATCH_AT_FRACTION, 0.5, PARAMDOC_SG_MATCH_AT_FRACTION);
	CHECK_PARSED(SG_PARTICLE_FRACTION, 0.95, PARAMDOC_SG_PARTICLE_FRACTION);
	CHECK_PARSED(FORCE_RLX_STEP, 0, PARAMDOC_FORCE_RLX_STEP);
        CHECK_PARSED(DT_HARD_BINARIES, 0, PARAMDOC_DT_HARD_BINARIES);
        CHECK_PARSED(HARD_BINARY_KT, 1, PARAMDOC_HARD_BINARY_KT);
#ifdef EXPERIMENTAL
	CHECK_PARSED(BH_LC_FDT, 0.0, PARAMDOC_BH_LC_FDT);
	CHECK_PARSED(AVEKERNEL, 20, PARAMDOC_AVEKERNEL);
#endif
	CHECK_PARSED(APSIDES_PRECISION, 1.0e-11, PARAMDOC_APSIDES_PRECISION);
	CHECK_PARSED(APSIDES_MAX_ITER, 100, PARAMDOC_APSIDES_MAX_ITER);
	CHECK_PARSED(APSIDES_CONVERGENCE, 5.e-13, PARAMDOC_APSIDES_CONVERGENCE);
	CHECK_PARSED(OVERWRITE_RVIR, 0., PARAMDOC_OVERWRITE_RVIR);
	CHECK_PARSED(OVERWRITE_Z, 0., PARAMDOC_OVERWRITE_Z);
	CHECK_PARSED(OVERWRITE_RTID, 0., PARAMDOC_OVERWRITE_RTID);
	CHECK_PARSED(OVERWRITE_MCLUS, 0., PARAMDOC_OVERWRITE_MCLUS);
	CHECK_PARSED(BSE_NETA, 0.50, PARAMDOC_BSE_NETA);
	CHECK_PARSED(BSE_BWIND, 0.00, PARAMDOC_BSE_BWIND);
	CHECK_PARSED(BSE_HEWIND, 0.50, PARAMDOC_BSE_HEWIND);
	CHECK_PARSED(BSE_WINDFLAG, 1, PARAMDOC_BSE_WINDFLAG);
	CHECK_PARSED(BSE_ALPHA1, 3.00, PARAMDOC_BSE_ALPHA1);
	CHECK_PARSED(BSE_LAMBDA, 0.50, PARAMDOC_BSE_LAMBDA);
	CHECK_PARSED(BSE_CEFLAG, 0, PARAMDOC_BSE_CEFLAG);
	CHECK_PARSED(BSE_TFLAG, 1, PARAMDOC_BSE_TFLAG);
	CHECK_PARSED(BSE_IFFLAG, 0, PARAMDOC_BSE_IFFLAG);
	CHECK_PARSED(BSE_WDFLAG, 1, PARAMDOC_BSE_WDFLAG);
	CHECK_PARSED(BSE_BHFLAG, 1, PARAMDOC_BSE_BHFLAG);
	CHECK_PARSED(BSE_NSFLAG, 1, PARAMDOC_BSE_NSFLAG);
	CHECK_PARSED(BSE_MXNS, 3.00, PARAMDOC_BSE_MXNS);
	CHECK_PARSED(BSE_BCONST, -3000.00, PARAMDOC_BSE_BCONST);
	CHECK_PARSED(BSE_CK, -1000.00, PARAMDOC_BSE_CK);
	CHECK_PARSED(BSE_IDUM, -999, PARAMDOC_BSE_IDUM);
	CHECK_PARSED(BSE_SIGMA, 265.00, PARAMDOC_BSE_SIGMA);
	CHECK_PARSED(BSE_BETA, 0.12500, PARAMDOC_BSE_BETA);
	CHECK_PARSED(BSE_EDDFAC, 1.00, PARAMDOC_BSE_EDDFAC);
	CHECK_PARSED(BSE_GAMMA, -1.00, PARAMDOC_BSE_GAMMA);
#undef CHECK_PARSED

	/* exit if something is not set */
	if (!allparsed) {
		exit(1);
	}
	
	fclose(parsedfp);
	
	/* read the number of stars and possibly other parameters */
	cmc_read_fits_file(INPUT_FILE, &cfd);
	clus.N_STAR = cfd.NOBJ;
	clus.N_BINARY = cfd.NBINARY;
	if (OVERWRITE_RVIR>0.) {
		cfd.Rvir= OVERWRITE_RVIR;
	}
	if (OVERWRITE_RTID>0.) {
		cfd.Rtid= OVERWRITE_RTID;
	}
	if (OVERWRITE_Z>0.) {
		cfd.Z= OVERWRITE_Z;
	}
	if (OVERWRITE_MCLUS>0.) {
		cfd.Mclus= OVERWRITE_MCLUS;
	}

	R_MAX = cfd.Rtid;
	METALLICITY = cfd.Z;

	clus.N_STAR_NEW = clus.N_STAR;
	/* add 2 * clus.N_BINARY for binary disruptions */
	N_STAR_DIM = 2 + clus.N_STAR + 2 * clus.N_BINARY;
	/* remember we can form binaries from tidal capture */
	N_BIN_DIM = 2 + clus.N_STAR / 2 + clus.N_BINARY;
	
	/* safety factors, so we don't have to worry about memory management/garbage collection */
	N_STAR_DIM = (long) floor(1.1 * ((double) N_STAR_DIM));
	N_BIN_DIM = (long) floor(1.1 * ((double) N_BIN_DIM));
	
	/*********************************************/
	/* allocation of memory for global variables */
	/*********************************************/
	
	
	//MPI3: Allocating only enough memory per processor.
	N_STAR_DIM_OPT = 2 + clus.N_STAR / procs + 2 * clus.N_BINARY / procs;
	N_STAR_DIM_OPT = (long) floor(1.1 * ((double) N_STAR_DIM_OPT));

	/* allocate memory for velocity dispersion array */
	sigma_array.n = 0;

	/* the main star array containing all star parameters */
#ifdef USE_MPI
	star = (star_t *) calloc(N_STAR_DIM_OPT, sizeof(star_t));
	sigma_array.r = (double *) calloc(N_STAR_DIM_OPT, sizeof(double));
	sigma_array.sigma = (double *) calloc(N_STAR_DIM_OPT, sizeof(double));
#else
	star = (star_t *) calloc(N_STAR_DIM, sizeof(star_t));
	sigma_array.r = (double *) calloc(N_STAR_DIM, sizeof(double));
	sigma_array.sigma = (double *) calloc(N_STAR_DIM, sizeof(double));
#endif

	
	/* the main binary array containing all binary parameters */
	binary = (binary_t *) calloc(N_BIN_DIM, sizeof(binary_t));
	
	/* quantities calculated for various lagrange radii */
	mass_r = (double *) malloc(MASS_PC_COUNT * sizeof(double));
	ave_mass_r = (double *) malloc(MASS_PC_COUNT * sizeof(double));
	no_star_r = (double *) malloc(MASS_PC_COUNT * sizeof(double));
	densities_r = (double *) malloc(MASS_PC_COUNT * sizeof(double));
	ke_rad_r = (double *) malloc(MASS_PC_COUNT * sizeof(double));
	ke_tan_r = (double *) malloc(MASS_PC_COUNT * sizeof(double));
	v2_rad_r = (double *) malloc(MASS_PC_COUNT * sizeof(double));
	v2_tan_r = (double *) malloc(MASS_PC_COUNT * sizeof(double));
	mass_pc = (double *) calloc(MASS_PC_COUNT, sizeof(double));
	mass_bins = (double *) calloc(NO_MASS_BINS, sizeof(double));
	multi_mass_r = (double **) malloc(NO_MASS_BINS * sizeof(double *));
	for(i=0; i<NO_MASS_BINS; i++){
		multi_mass_r[i] = (double *) malloc(MASS_PC_COUNT * sizeof(double));
	}
	
	/*======= Reading values for the Lagrange radii =======*/
	curr_mass = (char *) strtok(MASS_PC, ",; ");
	sscanf(curr_mass, "%lf", &mass_pc[0]);
	
	for (i=1; (curr_mass = (char *) strtok(NULL, " ,;")) != NULL; i++){
		sscanf(curr_mass, "%lf", &mass_pc[i]);
	}
	
	/*======= Reading values for the mass bins =======*/
	curr_mass = (char *) strtok(MASS_BINS, ",; ");
	sscanf(curr_mass, "%lf", &mass_bins[0]);

	for (i=1; (curr_mass = (char *) strtok(NULL, " ,;")) != NULL; i++){
		sscanf(curr_mass, "%lf", &mass_bins[i]);
	}

	/*======= Opening of output files =======*/
	sscanf("w", "%s", outfilemode);
	
	sprintf(outfile, "%s.lagrad.dat", outprefix);
	if ((lagradfile = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create output file \"%s\".\n", outfile);
		exit(1);
	}
	sprintf(outfile, "%s.dyn.dat", outprefix);
	if ((dynfile = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create output file \"%s\".\n", outfile);
		exit(1);
	}
	//Sourav:new file containing info at 10% lagrange radius
	sprintf(outfile, "%s.lagrad_10_info.dat", outprefix);
	if ((lagrad10file = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create output file \"%s\".\n", outfile);
		exit(1);
	}
	sprintf(outfile, "%s.avemass_lagrad.dat", outprefix);
	if ((ave_mass_file = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create output file \"%s\".\n", outfile);
		exit(1);
	}
	sprintf(outfile, "%s.nostar_lagrad.dat", outprefix);
	if ((no_star_file = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create output file \"%s\".\n", outfile);
		exit(1);
	}
	sprintf(outfile, "%s.rho_lagrad.dat", outprefix);
	if ((densities_file = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create output file \"%s\".\n", outfile);
		exit(1);
	}
	sprintf(outfile, "%s.ke_rad_lagrad.dat", outprefix);
	if ((ke_rad_file = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create output file \"%s\".\n", outfile);
		exit(1);
	}
	sprintf(outfile, "%s.ke_tan_lagrad.dat", outprefix);
	if ((ke_tan_file = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create output file \"%s\".\n", outfile);
		exit(1);
	}
	sprintf(outfile, "%s.v2_rad_lagrad.dat", outprefix);
	if ((v2_rad_file = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create output file \"%s\".\n", outfile);
		exit(1);
	}
	sprintf(outfile, "%s.v2_tan_lagrad.dat", outprefix);
	if ((v2_tan_file = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create output file \"%s\".\n", outfile);
		exit(1);
	}
	sprintf(outfile, "%s.centmass.dat", outprefix);
	if ((centmass_file = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create output file \"%s\".\n", outfile);
		exit(1);
	}

	sprintf(outfile, "%s.log", outprefix);
	if ((logfile = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create log output file \"%s\".\n", outfile);
		exit(1);
	}

	/* output files for binaries */
	/* general binary information */
	sprintf(outfile, "%s.bin.dat", outprefix);
	if ((binaryfile = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create binary file \"%s\".\n", outfile);
		exit(1);
	}
	/* file for binary interaction information */
	sprintf(outfile, "%s.binint.log", outprefix);
	if ((binintfile = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create binintlog file \"%s\".\n", outfile);
		exit(1);
	}
	
	/* File for parameters of escaping stars */
	sprintf(outfile, "%s.esc.dat", outprefix);
	if ((escfile = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create escapers file \"%s\".\n", outfile);
		exit(1);
	}
	/* print header */
	fprintf(escfile, "#1:tcount #2:t #3:m #4:r #5:vr #6:vt #7:r_peri #8:r_apo #9:Rtidal #10:phi_rtidal #11:phi_zero #12:E #13:J #14:id #15:binflag #16:m0[MSUN] #17:m1[MSUN] #18:id0 #19:id1 #20:a #21:e #22:startype #23:bin_startype0 #24:bin_startype1\n");

	/* Collision log file */
	sprintf(outfile, "%s.collision.log", outprefix);
	if ((collisionfile = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create collision log file \"%s\".\n", outfile);
		exit(1);
	}
	/* print header */
	fprintf(collisionfile, "# time interaction_type id_merger(mass_merger) id1(m1):id2(m2):id3(m3):... (r) type_merger type1 ...\n");

	/* Tidal capture log file */
	sprintf(outfile, "%s.tidalcapture.log", outprefix);
	if ((tidalcapturefile = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create tidal capture log file \"%s\".\n", outfile);
		exit(1);
	}
	/* print header */
	fprintf(tidalcapturefile, "# time interaction_type (id1,m1,k1)+(id2,m2,k2)->[(id1,m1,k1)-a,e-(id2,m2,k2)]\n");

	/* Stellar evolution merger log file */
	sprintf(outfile, "%s.semergedisrupt.log", outprefix);
	if ((semergedisruptfile = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create stellar evolution merger/disruption log file \"%s\".\n", outfile);
		exit(1);
	}
	/* print header */
	fprintf(semergedisruptfile, "# time interaction_type id_rem(mass_rem) id1(m1):id2(m2) (r)\n");

	/*Sourav: old star removal info file*/
	sprintf(outfile, "%s.removestar.log", outprefix);
	if ((removestarfile = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create old star removal log file \"%s\".\n", outfile);
		exit(1);
	}
	/*Sourav: print header*/
	fprintf(removestarfile, "#single destroyed: time star_id star_mass(MSun) star_age(Gyr) star_birth(Gyr) star_lifetime(Gyr)\n");
	fprintf(removestarfile, "#binary destroyed: time obj_id bin_id removed_comp_id left_comp_id m1(MSun) m2(MSun) removed_m(MSun) left_m(MSun) left_m_sing(MSun) star_age(Gyr) star_birth(Gyr) star_lifetime(Gyr)\n");

	/* Relaxation data file */
	sprintf(outfile, "%s.relaxation.dat", outprefix);
	if ((relaxationfile = fopen(outfile, outfilemode)) == NULL) {
		eprintf("cannot create relaxation data file \"%s\".\n", outfile);
		exit(1);
	}

	/* lagrange radii for multiple mass bins */
	mlagradfile = (FILE **) malloc((NO_MASS_BINS-1)*sizeof(FILE *));
	for(i=0; i<NO_MASS_BINS-1; i++){ /* NO_MASS_BINS need to be >=2 to be
							meaningful */
		sprintf(outfile, "%s.lagrad%ld-%g-%g.dat", outprefix, i,
							mass_bins[i], mass_bins[i+1]);
		if ((mlagradfile[i] = fopen(outfile, outfilemode)) == NULL) {
			eprintf("cannot create output file \"%s\".\n", outfile);
			exit(1);
		}
	}
        
	/* File that contains data for various definitions of core radii */
	if (WRITE_EXTRA_CORE_INFO) {
		sprintf(outfile, "%s.core.dat", outprefix);
		if ((corefile = fopen(outfile, outfilemode)) == NULL) {
			eprintf("cannot create output file \"%s\".\n", outfile);
			exit(1);
		}

		/* the core that contains no remnants */
		append_core_header(corefile, "norem", 0);
		fprintf(corefile, "\n");
	}
}

/* close buffers */
void close_buffers(void)
{
	int i;

	fclose(lagradfile);
	fclose(dynfile);
	fclose(lagrad10file);
	fclose(logfile);
	fclose(ave_mass_file);
	fclose(no_star_file);
	fclose(densities_file);
	fclose(ke_rad_file);
	fclose(ke_tan_file);
	fclose(v2_rad_file);
	fclose(v2_tan_file);
	fclose(centmass_file);
	fclose(escfile);
	fclose(collisionfile);
	fclose(tidalcapturefile);
	fclose(semergedisruptfile);
	fclose(relaxationfile);
	/*Sourav: closing the file I opened*/
	fclose(removestarfile);
	fclose(binaryfile);
	fclose(binintfile);

	for(i=0; i<NO_MASS_BINS-1; i++){
		fclose(mlagradfile[i]);
	}
        if (WRITE_EXTRA_CORE_INFO) {
          fclose(corefile);
        }
}

/* trap signals */
void trap_sigs(void)
{
	/* Catch some signals */
	signal(SIGINT, exit_cleanly);
	signal(SIGTERM, exit_cleanly);
	signal(SIGQUIT, exit_cleanly);
	signal(SIGUSR1, toggle_debugging);
	
	/* override GSL error handler */
	//gsl_set_error_handler(&sf_gsl_errhandler);
}

/* print handy script for converting output files to physical units */
void print_conversion_script(void)
{
	char dummystring[1024];
	FILE *ofp;
	/* BOOKMARK */

	sprintf(dummystring, "%s.conv.sh", outprefix);
	if ((ofp = fopen(dummystring, "w")) == NULL) {
		eprintf("cannot create convsersion script file \"%s\".\n", dummystring);
		exit(1);
	}
	
	fprintf(ofp, "#!/bin/bash\n");
	fprintf(ofp, "\n");
	fprintf(ofp, "# outfile prefix\n");
	fprintf(ofp, "outprefix=%s\n", outprefix);
	fprintf(ofp, "# code unit of mass (cgs)\n");
	fprintf(ofp, "massunitcgs=%g\n", units.m);
	fprintf(ofp, "# code unit of mass (M_sun)\n");
	fprintf(ofp, "massunitmsun=%g\n", units.m/MSUN);
	fprintf(ofp, "# code unit of stellar mass (cgs)\n");
	fprintf(ofp, "mstarunitcgs=%g\n", units.mstar);
	fprintf(ofp, "# code unit of stellar mass (M_Sun)\n");
	fprintf(ofp, "mstarunitmsun=%g\n", units.mstar/MSUN);
	fprintf(ofp, "# code unit of length (cgs)\n");
	fprintf(ofp, "lengthunitcgs=%g\n", units.l);
	fprintf(ofp, "# code unit of length (parsecs)\n");	
	fprintf(ofp, "lengthunitparsec=%g\n", units.l/PARSEC);
	fprintf(ofp, "# code unit of time (cgs)\n");
	fprintf(ofp, "timeunitcgs=%g\n", units.t * clus.N_STAR / log(GAMMA * clus.N_STAR));
	fprintf(ofp, "# code unit of time (Myr)\n");
	fprintf(ofp, "timeunitsmyr=%g\n", units.t * clus.N_STAR / log(GAMMA * clus.N_STAR) / (1.0e6 * YEAR));
	fprintf(ofp, "# N-body  unit of time (cgs)\n");
	fprintf(ofp, "nbtimeunitcgs=%g\n", units.t);
	fprintf(ofp, "# N-body unit of time (Myr)\n");
	fprintf(ofp, "nbtimeunitsmyr=%g\n", units.t / (1.0e6 * YEAR));
	fprintf(ofp, "\n");
	fprintf(ofp, "cat $outprefix.dyn.dat | grep -vE '^#' | awk '{print $1*'$timeunitsmyr', $8/$21}' > $outprefix.tmyr_rcrh.dat\n");
	fprintf(ofp, "prunedata.pl -d 30 $outprefix.tmyr_rcrh.dat > $outprefix.tmyr_rcrh-pruned.dat\n");
	fprintf(ofp, "\n");
	fprintf(ofp, "cat $outprefix.dyn.dat | grep -vE '^#' | awk '{print $1*'$timeunitsmyr', $25/$21}' > $outprefix.tmyr_rcnbrh.dat\n");
	fprintf(ofp, "prunedata.pl -d 30 $outprefix.tmyr_rcnbrh.dat > $outprefix.tmyr_rcnbrh-pruned.dat\n");
	fprintf(ofp, "\n");
	fprintf(ofp, "cat $outprefix.dyn.dat | grep -vE '^#' | awk '{print $1*'$timeunitsmyr', $7/(4.0/3.0*3.14159265*$8*'$lengthunitparsec')^3}' > $outprefix.tmyr_nc.dat\n");
	fprintf(ofp, "prunedata.pl -d 30 $outprefix.tmyr_nc.dat > $outprefix.tmyr_nc-pruned.dat\n");
	fprintf(ofp, "\n");
	fprintf(ofp, "cat $outprefix.dyn.dat | grep -vE '^#' | awk '{print $1*'$timeunitsmyr', $5}' > $outprefix.tmyr_m.dat\n");
	fprintf(ofp, "prunedata.pl -d 30 $outprefix.tmyr_m.dat > $outprefix.tmyr_m-pruned.dat\n");
	fprintf(ofp, "\n");
	fprintf(ofp, "cat $outprefix.bin.dat | grep -vE '^#' | awk '{print $1*'$timeunitsmyr', $11}' > $outprefix.tmyr_fbc.dat\n");
	fprintf(ofp, "prunedata.pl -d 30 $outprefix.tmyr_fbc.dat > $outprefix.tmyr_fbc-pruned.dat\n");
	fprintf(ofp, "\n");
	fprintf(ofp, "cat $outprefix.bin.dat | grep -vE '^#' | awk '{print $1*'$timeunitsmyr', $12}' > $outprefix.tmyr_fb.dat\n");
	fprintf(ofp, "prunedata.pl -d 30 $outprefix.tmyr_fb.dat > $outprefix.tmyr_fb-pruned.dat\n");
	fprintf(ofp, "\n");
	fprintf(ofp, "cat $outprefix.tmyr_nc.dat | awk '{print NR, $2}' > $outprefix.NR_nc.dat\n");
	fprintf(ofp, "cat $outprefix.tmyr_fbc.dat | awk '{print NR, $2}' > $outprefix.NR_fbc.dat\n");
	fprintf(ofp, "join $outprefix.NR_fbc.dat $outprefix.NR_nc.dat | awk '{print $2, $3}' > $outprefix.fbc_nc.dat\n");
	fprintf(ofp, "prunedata.pl -n 300 $outprefix.fbc_nc.dat > $outprefix.fbc_nc-pruned.dat\n");
	fprintf(ofp, "head -n 1 $outprefix.fbc_nc.dat > $outprefix.fbc_nc-arrow.dat\n");
	fprintf(ofp, "tail -n 1 $outprefix.fbc_nc.dat >> $outprefix.fbc_nc-arrow.dat\n");
	fclose(ofp);

	chmod(dummystring, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
}

/* routines for printing star/binary info in a unified log format */
char *sprint_star_dyn(long k, char string[MAX_STRING_LENGTH])
{
	snprintf(string, MAX_STRING_LENGTH, "(%ld,%.3g,%d)", 
#ifdef USE_MPI
		 star[k].id, star_m[k]*units.mstar/FB_CONST_MSUN, star[k].se_k);
#else
		 star[k].id, star[k].m*units.mstar/FB_CONST_MSUN, star[k].se_k);
#endif
	return(string);
}

char *sprint_bin_dyn(long k, char string[MAX_STRING_LENGTH])
{
	long bi=star[k].binind;

	snprintf(string, MAX_STRING_LENGTH, "[(%ld,%.3g,%d)-%.3g,%.3g-(%ld,%.3g,%d)]", 
		 binary[bi].id1, binary[bi].m1*units.mstar/FB_CONST_MSUN, binary[bi].bse_kw[0],
		 binary[bi].a*units.l/FB_CONST_AU, binary[bi].e,
		 binary[bi].id2, binary[bi].m2*units.mstar/FB_CONST_MSUN, binary[bi].bse_kw[1]);

	return(string);
}

void parse_snapshot_windows(char *param_string) {
  char *cur_window, *intern_window=NULL, *intern_param=NULL;
  char *cur_wstring, *cur_pstring;
  int j;
  strcpy(SNAPSHOT_WINDOWS, param_string);
  snapshot_window_count= 0;
  snapshot_windows= NULL;
  cur_wstring= param_string;
  while ((cur_window = strtok_r(cur_wstring,";", &intern_window))!= NULL) {
    snapshot_window_count++;
    snapshot_windows = (double *) realloc(snapshot_windows, 3*snapshot_window_count*sizeof(double));
    cur_wstring= NULL;
    for (j=0, cur_pstring=cur_window; j< 3; j++, cur_pstring=NULL) {
      char *wparam;
      int cur_wpidx;
      wparam= strtok_r(cur_pstring, ",", &intern_param);
      if (wparam==NULL) {
        eprintf("Error parsing snapshot window list.\n");
        eprintf("The current window is %s, and the config parameter is %s.\n", 
            cur_window, param_string);
        free_arrays();
        exit(-1);
      }
      cur_wpidx= (snapshot_window_count-1)*3+j;
      sscanf(wparam, "%lf", &(snapshot_windows[cur_wpidx]));
    }
  }

  dprintf("Finished parsing window list.\n");
  for (j=0; j< snapshot_window_count; j++) {
    dprintf("Window %i: ", j);
    dprintf("start %g, step %g, stop %g\n", snapshot_windows[3*j], snapshot_windows[3*j+1], snapshot_windows[3*j+2]);
  }
  snapshot_window_counters= (int *) calloc(snapshot_window_count, sizeof(int));
}

void print_snapshot_windows(void) {
  int i, step_counter;
  double start, stop, step, total_time;
  total_time = 0;

  if (!snapshot_window_count || !SNAPSHOTTING) return;

  if (strncmp(SNAPSHOT_WINDOW_UNITS, "Trel", 5)==0) {
    total_time= TotalTime;
  } else if (strncmp(SNAPSHOT_WINDOW_UNITS, "Gyr", 4)==0) {
    total_time= TotalTime * units.t * clus.N_STAR / log(GAMMA * clus.N_STAR) / YEAR/1e9;
  } else if (strncmp(SNAPSHOT_WINDOW_UNITS, "Tcr", 4)==0) {
    total_time= TotalTime * log(GAMMA * clus.N_STAR) / clus.N_STAR;
  } else {
    eprintf("Unrecognized unit %s.", SNAPSHOT_WINDOW_UNITS);
    exit_cleanly(-1);
  }

  for (i=0; i<snapshot_window_count; i++) {
    step_counter= snapshot_window_counters[i];
    start= snapshot_windows[i*3];
    step=  snapshot_windows[i*3+1];
    stop=  snapshot_windows[i*3+2];
    if (total_time>= start+step_counter*step && total_time<=stop) {
      char outfile[100];
      sprintf(outfile, "%s.w%02i_snap%04d.dat.gz", outprefix, i+1, step_counter+1);
      write_snapshot(outfile);
      snapshot_window_counters[i]++;
      dprintf("Wrote snapshot #%i for time window %i (%s) at time %g %s.", step_counter+1, i+1, outfile, 
          total_time, SNAPSHOT_WINDOW_UNITS);
    }
  }
}

int valid_snapshot_window_units(void) {
  int valid;

  valid=0;
  if (strncmp(SNAPSHOT_WINDOW_UNITS, "Trel", 5)==0) {
    valid=1;
  } else if (strncmp(SNAPSHOT_WINDOW_UNITS, "Gyr", 4)) {
    valid=1;
  } else if (strncmp(SNAPSHOT_WINDOW_UNITS, "Tcr", 4)) {
    valid=1;
  } 

  return (valid);
}

void write_snapshot(char *filename) {
  long i, j;
	j=0;

  if ((snapfile = (FILE *) gzopen(filename, "wb")) == NULL) {
    eprintf("cannot create 2D snapshot file %s\n", filename);
    exit_cleanly(1);
  }

  /* print useful header */
  gzprintf(snapfile, "# t=%.8g [code units]; All quantities below are in code units unless otherwise specified.\n", TotalTime);
  gzprintf(snapfile, "#1:id #2:m[MSUN] #3:r #4:vr #5:vt #6:E #7:J #8:binflag #9:m0[MSUN] #10:m1[MSUN] #11:id0 #12:id1 #13:a[AU] #14:e #15:startype #16:luminosity[LSUN] #17:radius[RSUN]  #18:bin_startype0 #19:bin_startype1 #20:bin_star_lum0[LSUN] #21:bin_star_lum1[LSUN] #22:bin_star_radius0[RSUN] #23:bin_star_radius1[RSUN] 24.star.phi\n");

  /* then print data */
  for (i=1; i<=clus.N_MAX; i++) {
    gzprintf(snapfile, "%ld %.8g %.8g %.8g %.8g %.8g %.8g ", 
        star[i].id, star[i].m * (units.m / clus.N_STAR) / MSUN, 
        star[i].r, star[i].vr, star[i].vt, 
        star[i].E, star[i].J);
    if (star[i].binind) {
      j = star[i].binind;
      gzprintf(snapfile, "1 %.8g %.8g %ld %ld %.8g %.8g ", 
          binary[j].m1 * (units.m / clus.N_STAR) / MSUN, 
          binary[j].m2 * (units.m / clus.N_STAR) / MSUN, 
          binary[j].id1, binary[j].id2,
          binary[j].a * units.l / AU, binary[j].e);
    } else {
      gzprintf(snapfile, "0 0 0 0 0 0 0 ");	
    }

    if (star[i].binind == 0) {
      gzprintf(snapfile, "%d %.8g %.8g -100 -100 -100 -100 -100 -100 ", 
          star[i].se_k, star[i].se_lum, star[i].rad * units.l / RSUN);
    } else {
      gzprintf(snapfile, "0 0 0 %d %d %.8g %.8g %.8g %.8g ",
          binary[j].bse_kw[0], binary[j].bse_kw[1], 
          binary[j].bse_lum[0], binary[j].bse_lum[1],
          binary[j].rad1*units.l/RSUN, binary[j].rad2*units.l/RSUN);
    }
    gzprintf(snapfile, "%0.12g\n", star[i].phi);
  }

  gzclose(snapfile);
}

void get_star_data(int argc, char *argv[], gsl_rng *rng)
{
	/* print version information to log file */
	//commenting out print_version temporarily for basic mpi
	//print_version(logfile);

	/* initialize the Search_Grid r_grid */
	//If we use the GPU code, we dont need the SEARCH_GRID. So commenting it out
	/*        if (SEARCH_GRID) { //Parallelize Search Grid
				 r_grid= search_grid_initialize(SG_POWER_LAW_EXPONENT, \
				 SG_MATCH_AT_FRACTION, SG_STARSPERBIN, SG_PARTICLE_FRACTION);
				 };
	 */
	/* initialize the root finder algorithm */
	/*	Commenting out for MPI
		q_root = gsl_root_fsolver_alloc (gsl_root_fsolver_brent);
	 */

	/* Commenting out for MPI
#ifdef DEBUGGING
	// create a new hash table for the star ids 
	star_ids= g_hash_table_new(g_int_hash, g_int_equal);
	// load a trace list 
	//load_id_table(star_ids, "trace_list");
#endif
	 */

	mpiInitGlobArrays();

	/* Set up initial conditions */
	//MPI2: can be done on all nodes? which means no need to broadcast star structure.
	//MPI2: This fn populates the star array with the the data obtained after parsing
	load_fits_file_data(); 

	/* set some important global variables */
	set_global_vars2();

	//Step 2:currently do on all nodes. But all procs will use same random numbers. So, comparing with host code might be a problem
	reset_rng_t113(IDUM);

	/* binary remainders */
	clus.N_MAX = clus.N_STAR;
	N_b = clus.N_BINARY;

	//MPI3: Check with Stefan if the sentinel is reqd.
	//star[clus.N_MAX+1].E = star[clus.N_MAX+1].J = 0.0;
}

void mpiInitGlobArrays()
{
#ifdef USE_MPI
	/*MPI2: Initializing and extracting global arrays that will be needed by all processors.*/
	//MPI2: Tested
	star_r = (double *) malloc(N_STAR_DIM * sizeof(double));
	star_m = (double *) malloc(N_STAR_DIM * sizeof(double));
	star_phi = (double *) malloc(N_STAR_DIM * sizeof(double));
#endif
}

void alloc_bin_buf()
{
#ifdef USE_MPI
	size_bin_buf = N_BIN_DIM / procs; //this is much more than what would be practically reqd. worst case scenario is N_BIN_DIM.

	//OPT: Modularize by declaring variables in main fn, and pass as argument to reqd functions.
	/* the local binary array */
	binary_buf = (binary_t *) calloc( size_bin_buf, sizeof(binary_t) );

	/* array to store the indices of the local binary members in the original binary array */
	num_bin_buf = (int *) calloc( size_bin_buf, sizeof(int) );
#endif
}

void collect_bin_data()
{
#ifdef USE_MPI
	int i, j, k, counter_new, cntr, destroyed_binaries, last_hole_idx=1, pass=0;
	MPI_Status stat;

	if(myid!=0)
	{
		i=0;
		while(num_bin_buf[i] != -1)
		{
			binary_buf[i] = binary[num_bin_buf[i]];
			i++;
		}

		MPI_Send(num_bin_buf, size_bin_buf, MPI_INT, 0, 0, MPI_COMM_WORLD);
		MPI_Send(binary_buf, size_bin_buf * sizeof(binary_t), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
	}
	else
		for(i=1; i<procs; i++)
		{
			MPI_Recv(num_bin_buf, size_bin_buf, MPI_INT, i, 0, MPI_COMM_WORLD, &stat);
			MPI_Recv(binary_buf, size_bin_buf * sizeof(binary_t), MPI_BYTE, i, 0, MPI_COMM_WORLD, &stat);
		
			j = 0;	
			while(num_bin_buf[j] != -1)
			{
				binary[num_bin_buf[j]] = binary_buf[j];
				j++;
			}
		}

	//MPI2: Gathering new stars.
	MPI_Gather( &N_b_NEW, 1, MPI_LONG, new_size, 1, MPI_LONG, 0, MPI_COMM_WORLD);
	binary_t *recv_buf;

	if(myid!=0)
	{
		if(N_b_NEW - N_b_OLD > 0)
			MPI_Send(&binary[N_b_OLD + 1], sizeof(binary_t) * (N_b_NEW - N_b_OLD), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
	}
	else
	{
		counter_new = N_b_NEW + 1;
		for(i=1; i<procs; i++)
		{
			new_size[i] -= N_b_OLD;
			if(new_size[i] > 0)
			{
				destroyed_binaries = 0;
				recv_buf = (binary_t *) calloc(new_size[i], sizeof(binary_t));
				MPI_Recv(recv_buf, sizeof(binary_t) * new_size[i], MPI_BYTE, i, 0, MPI_COMM_WORLD, &stat);

				for(cntr=0; cntr<new_size[i]; cntr++)
				{
					if(recv_buf[cntr].inuse == 0)
					{
						continue;
						destroyed_binaries++;
					}

					while (last_hole_idx <= N_b_OLD && binary[last_hole_idx].inuse)
						last_hole_idx++;

					if (last_hole_idx > N_b_OLD)
						break;

					binary[last_hole_idx] = recv_buf[cntr];
					for(j=clus.N_MAX+2; j <= clus.N_MAX_NEW; j++)
					{
						dprintf("binind of star %d is %ld\n", j, star[j].binind);
						if(star[j].binind == N_b_OLD + 1 + cntr)
						{
							star[j].binind = last_hole_idx;
							dprintf("ID = %d \tHOLE FOUND!!! at %d\tINSERTING STAR %d\n", i, last_hole_idx, j);
							pass = 1;
							break;
						}
					}
					if(pass == 0)
					{
						dprintf("Cant find single star correspoding to binary in node %d: local_idx=%ld nmax=%ld nmaxnew=%ld\n", myid, N_b_OLD + 1 + cntr, clus.N_MAX, clus.N_MAX_NEW);
						eprintf("FAILED!!!\n");
						exit_cleanly(-1);
					}
					pass = 0;
				}

				for(j=0; j<new_size[i]-cntr; j++)
				{
					if(recv_buf[j+cntr].inuse == 0)
					{
						continue;
						destroyed_binaries++;
					}

					binary[counter_new + j] = recv_buf[j+cntr];

					for(k=clus.N_MAX+2; k <= clus.N_MAX_NEW; k++)
						if(star[k].binind == N_b_OLD + 1 + cntr + j)
						{
							star[k].binind = counter_new + j;
							dprintf("ID = %d \tHOLE FOUND!!! at %d\tINSERTING STAR %d\n", i, counter_new + j, k);
							pass = 1;
							break;
						}
					if(pass == 0)
					{
						dprintf("Cant find single star correspoding to binary in node %d: local_idx=%ld nmax=%ld nmaxnew=%ld\n", myid, N_b_OLD + 1 + cntr + j, clus.N_MAX, clus.N_MAX_NEW);
						eprintf("FAILED!!!\n");
						exit_cleanly(-1);
					}
					pass = 0;
				}
				counter_new += new_size[i] - cntr - destroyed_binaries;
				free(recv_buf);
			}
		}
		N_b_OLD = counter_new - 1;
	}
	MPI_Bcast(&N_b_OLD, 1, MPI_LONG, 0, MPI_COMM_WORLD);
	N_b_NEW = N_b_OLD;
#endif
}

void distr_bin_data()
{
#ifdef USE_MPI
	int i, j, k;
	MPI_Status stat;

	if(myid==0)
	{
		for(i=1; i<procs; i++)
		{
			k=0;
			for(j=0; j<=clus.N_MAX; j++)
				if(star[j].binind > 0 && j >= Start[i] && j <= End[i])
				{
					num_bin_buf[k] = star[j].binind;
					k++;
				}
			num_bin_buf[k] = -1;

			MPI_Send(num_bin_buf, size_bin_buf, MPI_INT, i, 0, MPI_COMM_WORLD);
		}
	}
	else
		MPI_Recv(num_bin_buf, size_bin_buf, MPI_INT, 0, 0, MPI_COMM_WORLD, &stat);

	MPI_Bcast(binary, N_BIN_DIM * sizeof(binary_t), MPI_BYTE, 0, MPI_COMM_WORLD);
#endif
}

void mpi_files_merge(void)
{
#ifdef USE_MPI
	long i;
	MPI_Barrier(MPI_COMM_WORLD);
	if(myid==0)
	{
		char file_ext[64];
		cat_and_rm_files("cmc.parsed");
		cat_and_rm_files("lagrad.dat");
		cat_and_rm_files("dyn.dat");
		cat_and_rm_files("lagrad_10_info.dat");
		cat_and_rm_files("avemass_lagrad.dat");
		cat_and_rm_files("nostar_lagrad.dat");
		cat_and_rm_files("rho_lagrad.dat");
		cat_and_rm_files("ke_rad_lagrad.dat");
		cat_and_rm_files("ke_tan_lagrad.dat");
		cat_and_rm_files("v2_rad_lagrad.dat");
		cat_and_rm_files("v2_tan_lagrad.dat");
		cat_and_rm_files("centmass.dat");
		cat_and_rm_files("log");
		cat_and_rm_files("bin.dat");
		cat_and_rm_files("binint.log");
		cat_and_rm_files("esc.dat");
		cat_and_rm_files("collision.log");
		cat_and_rm_files("tidalcapture.log");
		cat_and_rm_files("semergedisrupt.log");
		cat_and_rm_files("removestar.log");
		cat_and_rm_files("relaxation.dat");
		for(i=0; i<NO_MASS_BINS-1; i++){
			sprintf(file_ext, "lagrad%ld-%g-%g.dat", i, mass_bins[i], mass_bins[i+1]);
			cat_and_rm_files(file_ext);
		}
		if (WRITE_EXTRA_CORE_INFO)
			cat_and_rm_files("core.dat");
	}
	//MPI2: Is barrier needed? Probably not.
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

void cat_and_rm_files(char* file_ext)
{
	char file_buffer[150], temp[150];
	int i;
	sprintf(file_buffer, "%s0.%s ", outprefix_bak, file_ext);
	for( i = 1; i < procs; ++i )
		sprintf( file_buffer, "%s%s%d.%s ", file_buffer, outprefix_bak, i, file_ext );
	strcpy(temp, file_buffer);
	sprintf( file_buffer, "cat %s> %s.%s", temp, outprefix_bak, file_ext);
	system( file_buffer );
	dprintf("MPI Files merging: lag file = %s\n", file_buffer);

	for( i = 0; i < procs; ++i )
	{
		sprintf( file_buffer, "rm %s%d.%s", outprefix_bak, i, file_ext);
		system( file_buffer );
	}	
}
