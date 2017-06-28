@echo off
cd ..\..\src\dep\
mkdir generated
cd scripts
echo %CD%
echo running perl scripts
perl cave_sprite_func.pl -o ../generated/cave_sprite_func.h
perl license2rtf.pl -o ../generated/license.rtf ../../license.txt
perl cave_tile_func.pl -o ../generated/cave_tile_func.h
perl neo_sprite_func.pl -o ../generated/neo_sprite_func.h
perl psikyo_tile_func.pl -o ../generated/psikyo_tile_func.h
perl toa_gp9001_func.pl -o ../generated/toa_gp9001_func.h
perl gamelist.pl -o ../generated/driverlist.h -l ../../gamelist.txt ../../burn/drv/capcom ../../burn/drv/cave ../../burn/drv/cps3 ../../burn/drv/dataeast ../../burn/drv/galaxian ../../burn/drv/irem ../../burn/drv/konami ../../burn/drv/megadrive ../../burn/drv/neogeo ../../burn/drv/pce ../../burn/drv/pgm ../../burn/drv/pst90s ../../burn/drv/pre90s ../../burn/drv/psikyo ../../burn/drv/sega ../../burn/drv/taito ../../burn/drv/toaplan ../../burn/drv/snes ../../burn/drv/sms

echo %CD%
echo building buildinfo
%1/bin/cl" build_details.cpp
build_details > ../generated/build_details.h
del build_details.exe
del build_details.obj

echo building ctv
cd ..
cd ..
cd burn
cd drv
cd capcom
echo %CD%
%1/bin/cl" ctv_make.cpp
ctv_make > ../../../dep/generated/ctv.h
del ctv_make.exe
del ctv_make.obj

echo building 68k generator
cd ..
cd ..
cd ..
cd cpu
cd m68k
echo %CD%
%1/bin/cl" /DINLINE="__inline static" m68kmake.c
m68kmake ../../dep/generated/ m68k_in.c
del m68kmake.exe
del m68kmake.obj
