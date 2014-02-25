all: html clean

html: index.html

index.html: i3blocks.1.ronn
	ronn -w -5 --style toc $< --pipe > index.html

i3blocks.1.ronn:
	git show master:$@ > $@

commit:
	git commit -m "update to `git describe --tags --always master`" index.html

clean:
	rm -f i3blocks.1.ronn
