# mgrep
print lines that match billions of strings efficiently

used as the backend for the Annotator at the National Center for Biomedical Ontology.

also, please checkout https://github.com/daimh/mgrepclient-java if mgrep is running under daemon mode

## Install
```
	$ git clone https://github.com/daimh/mgrep.git
	$ cd mgrep
	$ aclocal
	$ autoheader
	$ autoconf
	$ automake -a
	$ ./configure
	$ make
	$ sudo make install #install two files /usr/local/bin/mgrep and /usr/local/share/man/man1/mgrep.1
```

## Use
```
	$ mgrep -h
	$ mgrep match -h
```

## Citation 

[https://github.com/daimh/mgrep](https://github.com/daimh/mgrep)

## Copyright

Developed by [Manhong Dai](mailto:daimh@umich.edu)

Copyright © 2021 University of Michigan. License [AGPL](https://gnu.org/licenses/agpl-3.0.html): GNU AGPL version 3 or later 

This is free software: you are free to change and redistribute it.

There is NO WARRANTY, to the extent permitted by law.

## Acknowledgment

Fan Meng, Ph.D., Research Associate Professor, Psychiatry, UMICH

Ruth Freedman, MPH, former administrator of MNI, UMICH

Huda Akil, Ph.D., Director of MNI, UMICH

Stanley J. Watson, M.D., Ph.D., Director of MNI, UMICH
