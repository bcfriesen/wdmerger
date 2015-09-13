#include <cmath>

#include <iomanip>

#include <Castro.H>
#include <Castro_F.H>
#include <Geometry.H>

#include <Problem.H>
#include <Problem_F.H>

#include <Gravity.H>
#include <Gravity_F.H>

#include "buildInfo.H"

void
Castro::sum_integrated_quantities ()
{
    int finest_level  = parent->finestLevel();
    Real time         = state[State_Type].curTime();
    Real dt           = parent->dtLevel(0);

    if (time == 0.0) dt = 0.0; // dtLevel returns the next timestep for t = 0, so overwrite

    int step          = parent->levelSteps(0);
    Real mass         = 0.0;
    Real momentum[3]  = { 0.0 };
    Real rho_E        = 0.0;
    Real rho_e        = 0.0;
    Real rho_K        = 0.0;
    Real rho_phi      = 0.0;
    Real rho_phirot   = 0.0;

    Real gravitational_energy = 0.0; 
    Real kinetic_energy       = 0.0; 
    Real gas_energy           = 0.0;
    Real rotational_energy    = 0.0;
    Real internal_energy      = 0.0; 
    Real total_energy         = 0.0;

    Real angular_momentum[3]     = { 0.0 };
    Real moment_of_inertia[3][3] = { 0.0 };
    Real m_r_squared[3]          = { 0.0 };

    Real omega[3]       = { 0.0 };
    Real rot_mom[3]     = { 0.0 };
    Real rot_ang_mom[3] = { 0.0 };

#ifdef ROTATION
    omega[rot_axis-1] = 2.0 * M_PI / rotational_period;
#else
    int rot_axis = 3;
#endif

    Real total_E_grid = 0.0;
    Real mom_grid[3]  = { 0.0 };
    Real L_grid[3]    = { 0.0 };

    Real com[3]       = { 0.0 };
    Real lev_com[3]   = { 0.0 };
    Real com_vel[3]   = { 0.0 };

    Real mass_p       = 0.0;
    Real mass_s       = 0.0;

    Real lev_mass_p   = 0.0;
    Real lev_mass_s   = 0.0;

    Real com_p[3]     = { 0.0 };
    Real com_s[3]     = { 0.0 };
    Real lev_com_p[3] = { 0.0 };
    Real lev_com_s[3] = { 0.0 };
 
    Real vel_p[3] = { 0.0 };
    Real vel_s[3] = { 0.0 };
    
    Real lev_vel_p[3] = { 0.0 };
    Real lev_vel_s[3] = { 0.0 };

    Real separation = 0.0;
    Real angle = 0.0;

    Real vol_p[7] = { 0.0 };
    Real vol_s[7] = { 0.0 };

    Real lev_vol_p[7] = { 0.0 };
    Real lev_vol_s[7] = { 0.0 };

    Real rad_p[7] = { 0.0 };
    Real rad_s[7] = { 0.0 };

    // Average density of the stars.
    
    Real rho_avg_p = 0.0;
    Real rho_avg_s = 0.0;

    // Gravitational free-fall timescale of the stars.
    
    Real t_ff_p = 0.0;
    Real t_ff_s = 0.0;

    int single_star;

    // Gravitational wave amplitudes.
    
    Real h_plus_rot = 0.0;
    Real h_cross_rot = 0.0;

    Real h_plus_star = 0.0;
    Real h_cross_star = 0.0;

    Real h_plus_motion = 0.0;
    Real h_cross_motion = 0.0;

    // Number of species.
    
    int NumSpec;
    BL_FORT_PROC_CALL(GET_NUM_SPEC, get_num_spec)(&NumSpec);    

    // Species names and total masses on the domain.

    Real M_solar = 1.9884e33;
    
    Real species_mass[NumSpec] = { 0.0 };
    std::string species_names[NumSpec];
    
    std::string name1; 
    std::string name2;

    int index1;
    int index2;

    int datawidth     = 24;
    int dataprecision = 16;

    // Determine whether we're doing a single star simulation
    BL_FORT_PROC_CALL(GET_SINGLE_STAR,get_single_star)(single_star);

    // Update the problem center using the system bulk velocity
    BL_FORT_PROC_CALL(UPDATE_CENTER,update_center)(&time);

    // Determine the names of the species in the simulation.    

    for (int i = 0; i < NumSpec; i++) {
      species_names[i] = desc_lst[State_Type].name(FirstSpec+i);
      species_names[i] = species_names[i].substr(4,std::string::npos);
      species_mass[i]  = 0.0;	
    }
    
    for (int lev = 0; lev <= finest_level; lev++)
    {

      // Get the current level from Castro

      Castro& ca_lev = getLevel(lev);

      for ( int i = 0; i < 3; i++ ) {
        lev_com[i]    = ca_lev.locWgtSum("density", time, i);
        com[i]       += lev_com[i];
      }

      // Calculate total mass, momentum and energy of system.

      mass += ca_lev.volWgtSum("density", time);

      mom_grid[0] += ca_lev.volWgtSum("xmom", time);
      mom_grid[1] += ca_lev.volWgtSum("ymom", time);
      mom_grid[2] += ca_lev.volWgtSum("zmom", time);

      rho_E += ca_lev.volWgtSum("rho_E", time);
      rho_K += ca_lev.volWgtSum("kineng",time);
      rho_e += ca_lev.volWgtSum("rho_e", time);

#ifdef GRAVITY
      if ( do_grav and gravity->get_gravity_type() == "PoissonGrav" )
        rho_phi += ca_lev.volProductSum("density", "phiGrav", time);
#endif

#ifdef ROTATION
      if ( do_rotation )
	rho_phirot += ca_lev.volProductSum("density", "phiRot", time);
#endif            
      
      // Calculate total angular momentum on the grid using L = r x p

      for ( int i = 0; i < 3; i++ ) {

        index1 = (i+1) % 3; 
        index2 = (i+2) % 3;

        switch (i) {
          case 0 :
            name1 = "ymom"; name2 = "zmom"; break;
          case 1 :
            name1 = "zmom"; name2 = "xmom"; break;
          case 2 :
            name1 = "xmom"; name2 = "ymom"; break;
        }

        L_grid[i] = ca_lev.locWgtSum(name2, time, index1) - ca_lev.locWgtSum(name1, time, index2);

        angular_momentum[i] += L_grid[i];

      }

      // Add rotation source terms
#ifdef ROTATION
      if ( do_rotation ) {

        // Construct (symmetric) moment of inertia tensor

	for ( int i = 0; i < 3; i++ ) {
          m_r_squared[i] = ca_lev.locWgtSum2D("density", time, i, i);
        }

        for ( int i = 0; i <= 2; i++ ) {
	  for ( int j = 0; j <= 2; j++ ) {
            if ( i <= j ) {
              if ( i != j )  {
		moment_of_inertia[i][j] = -ca_lev.locWgtSum2D("density", time, i, j);
              }
              else
                moment_of_inertia[i][j] = m_r_squared[(i+1)%3] + m_r_squared[(i+2)%3];
            }
	    else
              moment_of_inertia[i][j] = moment_of_inertia[j][i];
          }
        }

        for ( int i = 0; i <= 2; i++ ) {

	  // Momentum source from motion IN rotating frame == omega x (rho * r)

          rot_mom[i] += omega[(i+1)%3]*lev_com[(i+2)%3] - omega[(i+2)%3]*lev_com[(i+1)%3];

	  // Now add quantities due to motion OF rotating frame

	  for ( int j = 0; j <=2; j++ )
            rot_ang_mom[i] += moment_of_inertia[i][j] * omega[j];

        }

      } 
#endif

      // Gravitational wave signal. This is designed to add to these quantities so we can send them directly.
      ca_lev.gwstrain(time, h_plus_rot, h_cross_rot, h_plus_star, h_cross_star, h_plus_motion, h_cross_motion);

      // Integrated mass of all species on the domain.      
      for (int i = 0; i < NumSpec; i++)
	species_mass[i] += ca_lev.volWgtSum("rho_" + species_names[i], time) / M_solar;
      
    }

    // Complete calculations for energy and momenta

    gravitational_energy = (-1.0/2.0) * rho_phi; // avoids double counting; CASTRO uses positive phi
    internal_energy = rho_e;
    kinetic_energy = rho_K;
    gas_energy = rho_E;
    rotational_energy = rho_phirot;
    total_E_grid = gravitational_energy + rho_E;
    total_energy = total_E_grid + rotational_energy;
    
    for (int i = 0; i < 3; i++) {
        momentum[i] = mom_grid[i] + rot_mom[i];
        angular_momentum[i] = L_grid[i] + rot_ang_mom[i];
    }

    // Compute the center of mass locations and velocities for the primary and secondary.
    // We'll start by predicting the current locations of their centers by taking the 
    // old locations and updating them using the old velocities and the time passed 
    // since the last update. Send it back to Fortran, then clear it out so we can update
    // using the full calculation.

    BL_FORT_PROC_CALL(GET_STAR_DATA,get_star_data)
      (com_p, com_s, vel_p, vel_s, &mass_p, &mass_s, &t_ff_p, &t_ff_s);

    for ( int i = 0; i < 3; i++ ) {
      com_p[i] += vel_p[i] * dt * sum_interval;
      com_s[i] += vel_s[i] * dt * sum_interval;
    }

    BL_FORT_PROC_CALL(SET_STAR_DATA,set_star_data)
      (com_p, com_s, vel_p, vel_s, &mass_p, &mass_s, &t_ff_s, &t_ff_s);

    for ( int i = 0; i < 3; i++ ) {
      com_p[i] = 0.0;
      com_s[i] = 0.0;
      vel_p[i] = 0.0;
      vel_s[i] = 0.0;
    }

    mass_p = 0.0;
    mass_s = 0.0;
    t_ff_p = 0.0;
    t_ff_s = 0.0;

    for (int lev = 0; lev <= finest_level; lev++)
    {

      getLevel(lev).wdCOM(time, lev_mass_p, lev_mass_s, lev_com_p, lev_com_s, lev_vel_p, lev_vel_s);

      mass_p += lev_mass_p;
      mass_s += lev_mass_s;

      for ( int i = 0; i < 3; i++ ) {
	com_p[i] += lev_com_p[i];
	com_s[i] += lev_com_s[i];
	vel_p[i] += lev_vel_p[i];
	vel_s[i] += lev_vel_s[i];
      }

    }

    // Complete calculations for center of mass quantities

    for ( int i = 0; i < 3; i++ ) {

      com[i]       = com[i] / mass;
      com_vel[i]   = momentum[i] / mass;

      if ( mass_p > 0.0 ) {
	com_p[i] = com_p[i] / mass_p;
        vel_p[i] = vel_p[i] / mass_p;
      }

      if (single_star != 1) {

	 if ( mass_s > 0.0 ) {
	   com_s[i] = com_s[i] / mass_s;
	   vel_s[i] = vel_s[i] / mass_s;
	 }

	 // Calculate the distance between the primary and secondary.

	 separation = sqrt( pow(com_p[0] - com_s[0], 2.0) + pow(com_p[1] - com_s[1], 2.0) + pow(com_p[2] - com_s[2], 2.0) );

	 // Calculate the angle between the x-axis and the line joining the two stars.
	 // We will assume that the motion along the rotation axis is negligible. 
	 // We can use the atan2 function to calculate the angle of a line 
	 // specified by two points with respect to the x-axis.

	 angle = atan2( com_s[(rot_axis+1)%3] - com_p[(rot_axis+1)%3], com_s[(rot_axis)%3] - com_p[(rot_axis)%3] ) * 180.0 / M_PI;
	 if (angle < 0.0)
	   angle += 360.0;

      }
    } 

    // Compute effective radii of stars at various density cutoffs

    for (int lev = 0; lev <= finest_level; lev++)
        for (int i = 0; i <= 6; ++i) {
	    getLevel(lev).volInBoundary(time, lev_vol_p[i], lev_vol_s[i], pow(10.0,i));
	    vol_p[i] += lev_vol_p[i];
	    vol_s[i] += lev_vol_s[i];
	}

    for (int i = 0; i <= 6; ++i) {
        rad_p[i] = std::pow(vol_p[i] * 3.0 / 4.0 / M_PI, 1.0/3.0);
	rad_s[i] = std::pow(vol_s[i] * 3.0 / 4.0 / M_PI, 1.0/3.0);
    }

    // Free-fall timescale ~ 1 / sqrt(G * rho_avg}

    Real Gconst;

    BL_FORT_PROC_CALL(GET_GRAV_CONST, get_grav_const)(&Gconst);

    if (mass_p > 0.0 && vol_p[0] > 0.0) {
      rho_avg_p = mass_p / vol_p[2];
      t_ff_p = sqrt(3.0 * M_PI / (32.0 * Gconst * rho_avg_p));
    }

    if (mass_s > 0.0 && vol_s[0] > 0.0) {
      rho_avg_s = mass_s / vol_s[2];
      t_ff_s = sqrt(3.0 * M_PI / (32.0 * Gconst * rho_avg_s));
    }

    // Send this information back to the Fortran probdata module

    BL_FORT_PROC_CALL(SET_STAR_DATA,set_star_data)
      (com_p, com_s, vel_p, vel_s, &mass_p, &mass_s, &t_ff_p, &t_ff_s);

    

    
    // Write data out to the log.

    if ( ParallelDescriptor::IOProcessor() )
    {

      // The data logs are only defined on the IO processor
      // for parallel runs, so the stream should only be opened inside.

      if (parent->NumDataLogs() > 0) {

	 std::ostream& grid_log = parent->DataLog(0);

	 if ( grid_log.good() ) {

	   // Write header row

	   if (time == 0.0) {

	     // Output the git commit hashes used to build the executable.

	     const char* castro_hash   = buildInfoGetGitHash(1);
	     const char* boxlib_hash   = buildInfoGetGitHash(2);
	     const char* wdmerger_hash = buildInfoGetBuildGitHash();

	     grid_log << "# Castro   git hash: " << castro_hash   << "\n";
	     grid_log << "# BoxLib   git hash: " << boxlib_hash   << "\n";
	     grid_log << "# wdmerger git hash: " << wdmerger_hash << "\n";

	     grid_log << std::setw(12)        << "#   TIMESTEP";
	     grid_log << std::setw(datawidth) << "     TIME              ";
	     grid_log << std::setw(datawidth) << "     DT                ";
	     grid_log << std::setw(datawidth) << " TOTAL ENERGY          ";
	     grid_log << std::setw(datawidth) << " TOTAL E GRID          ";
	     grid_log << std::setw(datawidth) << " GAS ENERGY            ";
	     grid_log << std::setw(datawidth) << " KIN. ENERGY           ";
	     grid_log << std::setw(datawidth) << " ROT. ENERGY           ";
	     grid_log << std::setw(datawidth) << " GRAV. ENERGY          ";
	     grid_log << std::setw(datawidth) << " INT. ENERGY           ";
	     grid_log << std::setw(datawidth) << " XMOM                  ";
	     grid_log << std::setw(datawidth) << " YMOM                  ";
	     grid_log << std::setw(datawidth) << " ZMOM                  ";
	     grid_log << std::setw(datawidth) << " XMOM GRID             ";
	     grid_log << std::setw(datawidth) << " YMOM GRID             ";
	     grid_log << std::setw(datawidth) << " ZMOM GRID             ";
	     grid_log << std::setw(datawidth) << " XMOM ROT.             ";
	     grid_log << std::setw(datawidth) << " YMOM ROT.             ";
	     grid_log << std::setw(datawidth) << " ZMOM ROT.             ";
	     grid_log << std::setw(datawidth) << " ANG. MOM. X           ";
	     grid_log << std::setw(datawidth) << " ANG. MOM. Y           ";
	     grid_log << std::setw(datawidth) << " ANG. MOM. Z           ";
	     grid_log << std::setw(datawidth) << " ANG. MOM. X GRID      ";
	     grid_log << std::setw(datawidth) << " ANG. MOM. Y GRID      ";
	     grid_log << std::setw(datawidth) << " ANG. MOM. Z GRID      ";
	     grid_log << std::setw(datawidth) << " ANG. MOM. X ROT.      ";
	     grid_log << std::setw(datawidth) << " ANG. MOM. Y ROT.      ";
	     grid_log << std::setw(datawidth) << " ANG. MOM. Z ROT.      ";
	     grid_log << std::setw(datawidth) << " MASS                  ";
	     grid_log << std::setw(datawidth) << " X COM                 ";
	     grid_log << std::setw(datawidth) << " Y COM                 ";
	     grid_log << std::setw(datawidth) << " Z COM                 ";
	     grid_log << std::setw(datawidth) << " X COM VEL             ";
	     grid_log << std::setw(datawidth) << " Y COM VEL             ";
	     grid_log << std::setw(datawidth) << " Z COM VEL             ";
	     grid_log << std::setw(datawidth) << " h_+ (rotation axis)   ";
	     grid_log << std::setw(datawidth) << " h_x (rotation axis)   ";
	     grid_log << std::setw(datawidth) << " h_+ (star axis)       ";
	     grid_log << std::setw(datawidth) << " h_x (star axis)       ";
	     grid_log << std::setw(datawidth) << " h_+ (motion axis)     ";
	     grid_log << std::setw(datawidth) << " h_x (motion axis)     ";

	     grid_log << std::endl;
	   }

	   // Write data for the present time

	   grid_log << std::fixed;

	   grid_log << std::setw(12)                                            << step;
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << time;
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << dt;

	   grid_log << std::scientific;

	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << total_energy;
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << total_E_grid;
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << gas_energy;
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << kinetic_energy;
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << rotational_energy;	  
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << gravitational_energy;
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << internal_energy;
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << momentum[0];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << momentum[1];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << momentum[2];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << mom_grid[0];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << mom_grid[1];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << mom_grid[2];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << rot_mom[0];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << rot_mom[1];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << rot_mom[2];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << angular_momentum[0];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << angular_momentum[1]; 
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << angular_momentum[2];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << L_grid[0];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << L_grid[1]; 
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << L_grid[2];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << rot_ang_mom[0];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << rot_ang_mom[1]; 
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << rot_ang_mom[2];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << mass;
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << com[0];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << com[1];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << com[2];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << com_vel[0];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << com_vel[1];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << com_vel[2];
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << h_plus_rot;
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << h_cross_rot;
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << h_plus_star;
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << h_cross_star;
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << h_plus_motion;
	   grid_log << std::setw(datawidth) << std::setprecision(dataprecision) << h_cross_motion;

	   grid_log << std::endl;
	 }
      }

      if (parent->NumDataLogs() > 1) {

	 std::ostream& star_log = parent->DataLog(1);

	 if ( star_log.good() ) {

	   if (time == 0.0) {

	     // Output the git commit hashes used to build the executable.

	     const char* castro_hash   = buildInfoGetGitHash(1);
	     const char* boxlib_hash   = buildInfoGetGitHash(2);
	     const char* wdmerger_hash = buildInfoGetBuildGitHash();

	     star_log << "# Castro   git hash: " << castro_hash   << "\n";
	     star_log << "# BoxLib   git hash: " << boxlib_hash   << "\n";
	     star_log << "# wdmerger git hash: " << wdmerger_hash << "\n";

	     star_log << std::setw(12)        << "#   TIMESTEP";
	     star_log << std::setw(datawidth) << "     TIME              ";
	     star_log << std::setw(datawidth) << "     DT                ";

	     star_log << std::setw(datawidth) << " WD DISTANCE           ";
	     star_log << std::setw(datawidth) << "   WD ANGLE            ";

	     star_log << std::setw(datawidth) << " PRIMARY X COM         ";
	     star_log << std::setw(datawidth) << " PRIMARY Y COM         ";
	     star_log << std::setw(datawidth) << " PRIMARY Z COM         ";
	     star_log << std::setw(datawidth) << " PRIMARY X VEL         ";
	     star_log << std::setw(datawidth) << " PRIMARY Y VEL         ";
	     star_log << std::setw(datawidth) << " PRIMARY Z VEL         ";
	     star_log << std::setw(datawidth) << " PRIMARY MASS          ";
	     star_log << std::setw(datawidth) << " PRIMARY AVG DENSITY   ";
	     star_log << std::setw(datawidth) << " PRIMARY T_FREEFALL    ";
	     for (int i = 0; i <= 6; ++i)
	       star_log << "  PRIMARY 1E" << i << " RADIUS    ";

	     star_log << std::setw(datawidth) << " SECONDARY X COM       ";
	     star_log << std::setw(datawidth) << " SECONDARY Y COM       ";
	     star_log << std::setw(datawidth) << " SECONDARY Z COM       ";
	     star_log << std::setw(datawidth) << " SECONDARY X VEL       ";
	     star_log << std::setw(datawidth) << " SECONDARY Y VEL       ";
	     star_log << std::setw(datawidth) << " SECONDARY Z VEL       ";
	     star_log << std::setw(datawidth) << " SECONDARY MASS        ";
	     star_log << std::setw(datawidth) << " SECONDARY AVG DENSITY ";
	     star_log << std::setw(datawidth) << " SECONDARY T_FREEFALL  ";
	     for (int i = 0; i <= 6; ++i)
	       star_log << "  SECONDARY 1E" << i << " RADIUS  ";

	     star_log << std::endl;

	   }

	   star_log << std::fixed;

	   star_log << std::setw(12)                                            << step;
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << time;
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << dt;

	   star_log << std::scientific;
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << separation;

	   star_log << std::fixed;
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << angle;

	   star_log << std::scientific;

	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << com_p[0];
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << com_p[1];
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << com_p[2];
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << vel_p[0];
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << vel_p[1];
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << vel_p[2];
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << mass_p;
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << rho_avg_p;
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << t_ff_p;
	   for (int i = 0; i <= 6; ++i)
	       star_log << std::setw(datawidth) << std::setprecision(dataprecision) << rad_p[i];

	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << com_s[0];
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << com_s[1];
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << com_s[2];
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << vel_s[0];
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << vel_s[1];
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << vel_s[2];
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << mass_s;
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << rho_avg_s;
	   star_log << std::setw(datawidth) << std::setprecision(dataprecision) << t_ff_s;
	   for (int i = 0; i <= 6; ++i)
	       star_log << std::setw(datawidth) << std::setprecision(dataprecision) << rad_s[i];

	   star_log << std::endl;

	 }
      }

      if (parent->NumDataLogs() > 2) {

	 std::ostream& species_log = parent->DataLog(2);

	 if ( species_log.good() ) {

	   if (time == 0.0) {

	     // Output the git commit hashes used to build the executable.

	     const char* castro_hash   = buildInfoGetGitHash(1);
	     const char* boxlib_hash   = buildInfoGetGitHash(2);
	     const char* wdmerger_hash = buildInfoGetBuildGitHash();

	     species_log << "# Castro   git hash: " << castro_hash   << "\n";
	     species_log << "# BoxLib   git hash: " << boxlib_hash   << "\n";
	     species_log << "# wdmerger git hash: " << wdmerger_hash << "\n";

	     species_log << std::setw(12)        << "#   TIMESTEP";
	     species_log << std::setw(datawidth) << "     TIME              ";
	     species_log << std::setw(datawidth) << "     DT                ";

	     for (int i = 0; i < NumSpec; i++)
	       species_log << std::setw(datawidth) << " Mass " + species_names[i];

	     species_log << std::endl;

	   }

	   species_log << std::fixed;

	   species_log << std::setw(12)                                            << step;
	   species_log << std::setw(datawidth) << std::setprecision(dataprecision) << time;
	   species_log << std::setw(datawidth) << std::setprecision(dataprecision) << dt;

	   species_log << std::scientific;

	   for (int i = 0; i < NumSpec; i++)
	     species_log << std::setw(datawidth) << std::setprecision(dataprecision) << species_mass[i];

	   species_log << std::endl;	   
	   
	 }
      }

    }
}
