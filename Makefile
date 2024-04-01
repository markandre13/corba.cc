all:
	git submodule init
	git submodule update

pull:
	git pull --rebase
	git submodule update --remote --rebase --recursive
