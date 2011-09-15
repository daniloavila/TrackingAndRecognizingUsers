if [ -z "$1" ]; then
	echo 'Passe o caminho para o jar do uos_core...'
else
	java -Djava.library.path=lib/ -jar $1
fi
