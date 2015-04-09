-- index.lua
-- responsible for sending command to marlin

local function sendmovex(connection, amount)

   -- send gcode
   print('MOVE '.. amount.. ' 0 0')

   -- Send back JSON response.
   connection:send("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\Cache-Control: private, no-store\r\n\r\n")
   connection:send('{"error":0, "message":"OK"}')

end

local function sendmovey(connection, amount)

   -- send gcode
   print('MOVE 0 '.. amount.. ' 0')

   -- Send back JSON response.
   connection:send("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\Cache-Control: private, no-store\r\n\r\n")
   connection:send('{"error":0, "message":"OK"}')

end

local function sendmovez(connection, amount)

   -- send gcode
    print('MOVE 0 0 '.. amount)

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

return function (connection, args)
     
   if args.movex then sendmovex(connection, args.movex)  
   elseif args.movey then sendmovey(connection, args.movey)
   elseif args.movez then sendmovez(connection, args.movez)
   elseif args.gettemp then gettemp(connection)
   elseif args.heater then settemp(connection, args.heater,args.settemp)
   else
      connection:send("HTTP/1.0 400 OK\r\nContent-Type: application/json\r\Cache-Control: private, no-store\r\n\r\n")
      connection:send('{"error":-1, "message":"Bad request"}')
   end
end
