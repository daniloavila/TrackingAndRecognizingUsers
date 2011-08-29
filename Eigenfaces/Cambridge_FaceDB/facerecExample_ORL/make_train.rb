# 12 personM Cambridge_FaceDB/s12/10.pgm

alfabeto = ["A","B","C","D","E","F","G","H","L","M","N", "O","P","Q","R","S","T","U","V","X","Z"]

(1..17).each do |i|

	str = "#{23+i} personA#{alfabeto[i]} Cambridge_FaceDB/s#{23+i}/"

	(1..10).each do |j|
		puts str + "#{j}pgm"
	end


end
