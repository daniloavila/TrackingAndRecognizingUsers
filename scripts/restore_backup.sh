if [ -z "$1" ]; then
	echo 'Passe o nome do backup q deseja restaurar'
else
	cd ../Eigenfaces/backup
	tar -xvf data_$1.tar
	mv out_averageImage.bmp ../
	mv out_eigenfaces.bmp ../
	mv facedata.xml ../
	mv train.txt ../
	rm -rf ../data/*
	mv data/* ../data/
	rm -rf data/
	rm -rf ../Cambridge_FaceDB/*
	mv Cambridge_FaceDB/* ../Cambridge_FaceDB/
	rm -rf Cambridge_FaceDB/
fi
