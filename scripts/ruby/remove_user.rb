
class RemoveUser

	attr_reader :name

	def initialize name, file
		@name = name
		@lines = File.open(file).read.split("\n")

		path = file.split('/')
		path.delete(path.last)

		data_path = ''
		path.each {|dir| data_path << "#{dir}/"}

		#deletando o facedata.xml que sera gerado novamente ao treinar o algoritmo.
		begin
			File.delete data_path + "facedata.xml" if Dir.entries(data_path).contain
		rescue
		end

		data_path << 'data/'

		@entries = Dir.entries data_path

	end

	def remove 
		index_update_train_file

		@lines.each {|line| lines.delete(line) unless line.match("#{@name}").nil?} #deletando as linhas dentro do arquivo

		files = entries.select {|entrie| entrie.match(/\S#{@name}\S/)}

		index_update_files(user_index files.first) #corrigindo index dos outros arquivos

		delete_all files #deletando as imagens da pessoas

	end

	def index_update_train_file 
		lines = @lines.select {|line| line.match("#{@name}")}
		index = index lines.first

		update_lines = @lines.select{|line| index(line) > index}
		update_lines.each do |line|
			new_line = line.split
			new_line.first = (new_line.first.to_i - 1).to_s
			new_line.last = new_line.last.split("_")
			new_line.last.first = new_line.split("/")
			new_line.last.first.last = new_line.last.first.last -1
			new_line.last.first.join!
			new_line.last.join!
			new_line.join(" ")
		end
	end

	def index line
		line.split(" ").first
	end

	def index_update_file  index
		files = @entries.select{|entrie| user_index(entrie) > index}

		files.each do |file|
			new_name = file.split('_')
			new_name.first = (new_name.first.to_i - 1).to_s
			File.rename(file, new_name.join('_'))
		end

	end

	private 
	def self.user_index file
		file.split("_").first
	end

	def self.delete_all files
		files.each {|file| File.delete file}
	end

end


user = RemoveUser.new "tntales", "../../Bin/Release/train.txt"
user.remove
