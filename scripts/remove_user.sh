if [$1 =  '']; then
	echo 'Passe o nome do usuário que deseja remover do sistema'
else
	ruby ruby/remove_user.rb $1
	cd ..
	./register train
fi
