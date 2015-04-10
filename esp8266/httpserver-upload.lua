
local boundary = nil
local content_length =0
local filename = nil
local state = 'header'
 
local function processPayload(connection, payload)
	--process line by line
	for line in string.gmatch(payload,"[^\r\n]+") do 
		-- header
		if state == 'header' then
			-- boundary decl
			nBegin,nEnde = string.find(line,"boundary=",1,true)
			if nBegin then boundary = string.sub(line,nEnde+1) end	
			-- length
			nBegin,nEnde = string.find(line,"Content-Length:",1,true)
			if nBegin then content_length = string.sub(line,nEnde) end	
			-- boundary
			if boundary then
				nBegin,nEnde = string.find(line,boundary,1,true)
				if nBegin then state = 'file_header' end
			end
		elseif state == 'file_header' then
			-- filename
			nBegin,nEnde = string.find(line,"filename=",1,true)
			if nBegin then filename = string.sub(line,nEnde+1) end	
			
			-- TODO correct start of data ?
			nBegin,nEnde = string.find(line,"Content-Type:",1,true)
			if nBegin then 
				state = 'data' 
				print("BEGIN OF FilE "..filename)
			end	
		elseif state == 'data' then
			-- check for boundary
			nBegin,nEnde = string.find(line,boundary,1,true)
			if nBegin then 
				print("END OF FILE !")
				connection:send("HTTP/1.0 200 OK \r\nServer: nodemcu-httpserver\r\nContent-Type:  text/html \r\nConnection: close\r\n\r\n")
				state = 'header'
			else
				-- output data for testing
				print(line)
			end
		end
		
		
	end
	
end

return function (connection, payload)

	state = 'header'
	boundary = nil
	filename = nil
	content_length =0
	connection:on("receive", processPayload)
	
    processPayload(connection,payload)
    
	 
	 -- connection:send("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\Cache-Control: private, no-store\r\n\r\n")
 
end