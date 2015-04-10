-- index.lua
-- responsible for sending command to marlin

local function sendmove(connection, amount,axis,speed)

   -- send gcode
   if axis == "1" then print('MOVE '.. amount..' 0 0 0 '..speed)
   elseif axis == "2" then print('MOVE 0 '.. amount..' 0 0 '..speed)
   elseif axis == "3" then print('MOVE 0 0 '.. amount..' 0 '..speed) 
   elseif axis == "4" then print('MOVE 0 0 0 '.. amount..' '..speed) 
   end

   -- Send back JSON response.
   connection:send("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\Cache-Control: private, no-store\r\n\r\n")
   connection:send('{"error":0, "message":"OK"}')

end



local function gettemp(connection)

	-- request temps
	uart.on("data", "\n", 
      function(data)
	    -- Send back JSON response.
        connection:send("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\Cache-Control: private, no-store\r\n\r\n")
        connection:send(data);
        uart.on("data") 
    end, 0)
	print('GETTEMP')
	
end

local function settemp(connection, heater, temp)

   -- send gcode
   print('SETTEMP ' .. heater .. ' ' .. temp)

   -- Send back JSON response.
   connection:send("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\Cache-Control: private, no-store\r\n\r\n")
   connection:send('{"error":0, "message":"OK"}')

end

local function home(connection, axis)

  -- send gcode
   print('HOME ' .. axis)

   -- Send back JSON response.
   connection:send("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\Cache-Control: private, no-store\r\n\r\n")
   connection:send('{"error":0, "message":"OK"}')
end

local function upload(connection, name, data)
	
	print('UPLOAD ')
	-- Send back JSON response.
	connection:send("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\Cache-Control: private, no-store\r\n\r\n")
	connection:send('{"error":0, "message":"OK"}')
end

return function (connection, args)
     
   if args.move then sendmove(connection, args.move,args.axis)  
   elseif args.gettemp then gettemp(connection)
   elseif args.heater then settemp(connection, args.heater,args.settemp)
   elseif args.home then home(connection,args.home)
   elseif args.upload then upload(connection,args.upload,args.data)
   else
      connection:send("HTTP/1.0 400 OK\r\nContent-Type: application/json\r\Cache-Control: private, no-store\r\n\r\n")
      connection:send('{"error":-1, "message":"Bad request"}')
   end
end
