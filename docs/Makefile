admin.ps:admin.tex webuser.man yapp.man
	touch admin.ind
	latex admin.tex
	latex admin.tex
	makeindex admin
	latex admin.tex
	dvips -o admin.ps admin
