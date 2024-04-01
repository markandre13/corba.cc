all:
	git submodule init
	git submodule update

update:
	git pull --rebase
	git submodule update --remote --rebase --recursive
