#!/bin/bash

if [ -f src/dep/generated/driverlist.h ]; then
   rm -rf src/dep/generated
fi

if [ -d src/dep/generated ]; then
   echo 'Directory 'src/dep/generated' already exists, skipping creation...'
else
   mkdir src/dep/generated
fi

#generate gamelist.txt and src/dep/generated/driverlist.h
perl src/dep/scripts/gamelist.pl -o src/dep/generated/driverlist.h -l gamelist.txt \
src/burn/drv/capcom \
src/burn/drv/cave \
src/burn/drv/cps3 \
src/burn/drv/dataeast \
src/burn/drv/galaxian \
src/burn/drv/irem \
src/burn/drv/konami \
src/burn/drv/megadrive \
src/burn/drv/neogeo \
src/burn/drv/pce \
src/burn/drv/pgm \
src/burn/drv/pre90s \
src/burn/drv/psikyo \
src/burn/drv/pst90s \
src/burn/drv/sega \
src/burn/drv/snes \
src/burn/drv/taito \
src/burn/drv/toaplan

#generate src/dep/generated/neo_sprite_func.h and src/dep/generated/neo_sprite_func_table.h
perl src/dep/scripts/neo_sprite_func.pl -o src/dep/generated/neo_sprite_func.h

#generate src/dep/generated/psikyo_tile_func.h and src/dep/generated/psikyo_tile_func_table.h
perl src/dep/scripts/psikyo_tile_func.pl -o src/dep/generated/psikyo_tile_func.h

#generate src/dep/generated/cave_sprite_func.h and src/dep/generated/cave_sprite_func_table.h
perl src/dep/scripts/cave_sprite_func.pl -o src/dep/generated/cave_sprite_func.h

#generate src/dep/generated/cave_tile_func.h and src/dep/generated/cave_tile_func_table.h
perl src/dep/scripts/cave_tile_func.pl -o src/dep/generated/cave_tile_func.h

#generate src/dep/generated/toa_gp9001_func.h and src/dep/generated/toa_gp9001_func_table.h
perl src/dep/scripts/toa_gp9001_func.pl -o src/dep/generated/toa_gp9001_func.h

g++ -o pgm_sprite_create src/burn/drv/pgm/pgm_sprite_create.cpp
./pgm_sprite_create > src/dep/generated/pgm_sprite.h

#compile m68kmakeecho 
gcc -o m68kmake src/cpu/m68k/m68kmake.c

#create m68kops.h with m68kmake
./m68kmake src/cpu/m68k/ src/cpu/m68k/m68k_in.c

g++ -o  ctvmake src/burn/drv/capcom/ctv_make.cpp
./ctvmake > src/dep/generated/ctv.h
