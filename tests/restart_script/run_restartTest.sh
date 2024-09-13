#!/bin/sh

# moves to starting directory
cd ../../srcPython

# runs Aether Restarts script
python aetherRestarts.py -restarts=2 -minutes=10

# plot output of restarted run
cd ../tests/restarts/run.halves/UA/output

~/bin/postAether.py -alt=-1 -rm
## [O]:
~/bin/aether_plot_simple.py -var=density_O -alt=250 3DALL_20110320_001000.nc
## Tn:
~/bin/aether_plot_simple.py -var=Temperature_neutral -alt=250 3DALL_20110320_001000.nc
## [e-]
~/bin/aether_plot_simple.py -var=density_e- -alt=250 3DALL_20110320_001000.nc

# plot output of whole run
cd ../../../run.whole/UA/output

~/bin/postAether.py -alt=-1 -rm
## [O]:
~/bin/aether_plot_simple.py -var=density_O -alt=250 3DALL_20110320_001000.nc
## Tn:
~/bin/aether_plot_simple.py -var=Temperature_neutral -alt=250 3DALL_20110320_001000.nc
## [e-]
~/bin/aether_plot_simple.py -var=density_e- -alt=250 3DALL_20110320_001000.nc

# you can then look at:
# restarted run results: run.halves/UA/output/*.png
# whole run results: run.whole/UA/output/*.png
# using whatever viewer you have
# and compare to plots and logs in tests/restart_script