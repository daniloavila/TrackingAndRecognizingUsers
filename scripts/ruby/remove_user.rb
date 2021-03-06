class RemoveUser

	attr_reader :name

	def initialize name, file
		@name = name
		@train = File.open(file)
		@lines = @train.read.split("\n")

		path = file.split('/')
		path.delete(path.last)

		@file_path = ''
		path.each {|dir| @file_path << "#{dir}/"}

		@data_path = @file_path.clone
		@data_path << 'data/'

		@entries = Dir.entries @data_path

	end

	def remove 
		user_lines = @lines.select {|line| line.match("#{@name}")}

		return "Usuário não cadastrado" if user_lines.nil? or user_lines.empty?

		#deletando o facedata.xml que sera gerado novamente ao treinar o algoritmo.
		begin
			File.delete(@file_path + "facedata.xml") 
			File.delete(@file_path + "out_averageImage.bmp")
			File.delete(@file_path + "out_eigenfaces.bmp")
		rescue
		end

		lines = index_update_train_file
		File.delete @file_path + "train.txt"
		@train = File.new(@file_path + "train.txt", "w+")
		@train.write(lines + "\n")

		files = @entries.select {|entrie| entrie.match(/\S#{@name}\S/)}
		index_update_file(user_index(files.first)) #corrigindo index dos outros arquivos
		files.map!{|file| @data_path + file}
		delete_all files #deletando as imagens da pessoas

		return "Remoção do usuário #{@name} realizada com sucesso."
	end

	def index_update_train_file 
		lines = @lines.select {|line| line.match("#{@name}")}
		user_index = index lines.first

		update_lines = @lines.select{|line| index(line).to_i > user_index.to_i}
		new_lines = @lines.select{|line| index(line).to_i < user_index.to_i}

		update_lines.each do |line|
			#separando por espaços
			new_line = line.split 

			#atualizando novo index
			new_index = index line
			new_line[0] = (new_line.first.to_i - 1).to_s
			new_line[-1] = new_line.last.split("_")
			new_line[-1][0] = new_line[-1][0].split("/")
			new_index = new_line[-1][0][-1].to_i - 1
			new_line[-1][0][-1] = new_index
			new_line[-1][0] = new_line[-1][0].join("/")
			new_line[-1] = new_line[-1].join("_")
			new_line = new_line.join(" ")
			new_lines << new_line
		end

		new_lines.join("\n")

	end

	def index line
		line.split(" ").first
	end

	def index_update_file  index
		files = @entries.select{|entrie| user_index(entrie).to_i > index.to_i}

		files.each do |file|
			new_name = file.split('_')
			new_name[0] = (new_name.first.to_i - 1).to_s
			File.rename(@data_path + file, @data_path + new_name.join('_'))
		end

	end

	def user_index file
		file.split("_").first
	end

	def delete_all files
		files.each {|file| File.delete file}
	end

end

user = RemoveUser.new ARGV.first , "../Eigenfaces/train.txt"
puts user.remove	
