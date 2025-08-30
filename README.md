# mgrep

**mgrep** efficiently prints lines that match **billions of strings**.  

While Google can search billions of web pages for a keyword in a split second, **mgrep** is optimized for the opposite task: searching billions of keywords within a short piece of text in a split second.  

It has been used as the backend for the [Annotator at the National Center for Biomedical Ontology](https://bioportal.bioontology.org/annotator) since 2009.  

If you are running **mgrep** in daemon mode, check out the Java client here: [mgrepclient-java](https://github.com/daimh/mgrepclient-java).  


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

Copyright © 2025 University of Michigan. License [AGPL](https://gnu.org/licenses/agpl-3.0.html): GNU AGPL version 3 or later 

This is free software: you are free to change and redistribute it.

There is NO WARRANTY, to the extent permitted by law.

## Acknowledgment

Fan Meng, Ph.D., Research Associate Professor, Psychiatry, UMICH  
Ruth Freedman, MPH, former administrator of MNI, UMICH  
Huda Akil, Ph.D., Director of MNI, UMICH  
Stanley J. Watson, M.D., Ph.D., Director of MNI, UMICH  
