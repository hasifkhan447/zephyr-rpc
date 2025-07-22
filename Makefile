arch = stm32f746g_disco
install_point = ~/External/Documents/zephyr/zephyr/

proj_dir = $(shell pwd)

build:
	cd $(install_point); west -v build -b $(arch) $(proj_dir) -d $(proj_dir)/../build;

buildp:
	cd $(install_point); west build -b $(arch) -p always $(proj_dir) -d $(proj_dir)/../build;

run:
	cd $(install_point); west build -b $(arch) $(proj_dir) --build-dir $(proj_dir)/../build -t run;

flash:
	cd $(install_point); west flash --build-dir $(proj_dir)/../build;










