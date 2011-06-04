if [$1 =  '']; then
	echo 'passe o numero da nova versão como parâmetro'
else
	tar -cvf backup/data_$1.tar out_averageImage.bmp out_eigenfaces.bmp data/* facedata.xml train.txt
fi
