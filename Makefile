all:
	git submodule init
	git submodule update

update:
	git submodule update --remote --rebase --recursive
