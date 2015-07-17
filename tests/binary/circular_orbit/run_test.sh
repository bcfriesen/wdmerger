source $WDMERGER_HOME/job_scripts/run_utils.sh

# Problem-specific variables

mass_P=0.90
mass_S=0.90

castro_rotational_period=100.0
stop_time=$castro_rotational_period

num_periods=50

final_stop_time=$(echo "$(num_periods) * $(castro_rotational_period)" | bc -l)

# Loop over the resolutions in question

for gs in 1 2 3 4
do
  for rs in 0 1 2 3 4
  do
    for ncell in 128 256
    do
      dir=$results_dir/gs$gs/rs$rs/n$ncell

      # First do the relevant updates in the inputs file.
      
      amr_n_cell="$ncell $ncell $ncell"
      castro_grav_source_type=$gs

      if [ $rs == 0 ]; then
	  castro_do_rotation=0
	  orbital_kick=T
      else
	  castro_do_rotation=1
	  castro_rot_source_type=$rs
	  orbital_kick=F
      fi

      # For this test we want to perform many orbits in succession. It is not feasible
      # to try to do all of them in one run. Therefore our strategy will be to do
      # one orbital period at a time. Each time we run, we check if we have yet reached
      # the desired final stopping time, which is the rotational period multiplied by the
      # number of desired orbits. If not, we update the stopping time in the inputs
      # to be increased by one rotational period, and then re-submit the job.
      # Also, make sure we completed the last orbit successfully.
      # This step is only necessary if we have already started the job initially.

      if [ -e $dir/$inputs ]; then

	checkpoint=$(get_last_checkpoint $dir)
        time=$(awk 'NR==3' $dir/$checkpoint/Header)

        if [ $(echo "$time < $final_stop_time" | bc -l) -eq 1 ]; then

	    # Now we have two cases. If we're less than one orbital period away, set 
	    # the stopping time equal to the final stopping time.
	    # Otherwise, add a full orbital period.

	    if [ $(echo "$time > ($final_stop_time - $castro_rotational_period)" | bc -l) -eq 1 ]; then
		new_time=$final_stop_time
	    else
		new_time=$(echo "$time + $castro_rotational_period" | bc -l)
	    fi

	    # While we are here, we don't want to do too many orbits for the options
	    # we do not prefer. We will stop at a single orbit for everytning but our
	    # preferred options in both the rotating and inertial frames.

	    new_time_flag=$(echo "$new_time >= $castro_rotational_period" | bc -l)

	    if [ $new_time_flag -eq 1 ]; then
		if [[ $gs -ne 4 ]]; then
		    continue
		fi
		if [[ $rs -ne 0 ]] && [[ $rs -ne 4 ]]; then
		    continue
		fi
	    fi

	    stop_time=$new_time

        fi

      fi

      if [ $MACHINE == "BLUE_WATERS" ]; then
	  if   [ $ncell == 64  ]; then
	      nprocs=32
	      walltime=00:45:00
	  elif [ $ncell == 128 ]; then
	      nprocs=128
	      walltime=6:00:00
	  elif [ $ncell == 256 ]; then
	      nprocs=1024
	      walltime=12:00:00
	  fi
      fi

      run $dir $nprocs $walltime
    done
  done
done