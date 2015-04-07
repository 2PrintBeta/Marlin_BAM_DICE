-- garage_door_opener.lua
-- Part of nodemcu-httpserver, example.
-- Author: Marcos Kirsch

local function sendmovex(connection, amount)

   -- send gcode
   print('Move X:', amount)

   -- Send back JSON response.
   connection:send("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\Cache-Control: private, no-store\r\n\r\n")
   connection:send('{"error":0, "message":"OK"}')

end

local function sendmovey(connection, amount)

   -- send gcode
   print('Move Y:', amount)

   -- Send back JSON response.
   connection:send("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\Cache-Control: private, no-store\r\n\r\n")
   connection:send('{"error":0, "message":"OK"}')

end

local function sendmovez(connection, amount)

   -- send gcode
   print('MOVE Z:', amount)

   -- Send back JSON response.
   connection:send("HTTP/1.0 200 OK\r\nContent-Type: application/json\r\Cache-Control: private, no-store\r\n\r\n")
   connection:send('{"error":0, "message":"OK"}')

end

return function (connection, args)
     
   if args.movex then sendmovex(connection, args.movex)  
   elseif args.movey then sendmovey(connection, args.movey)
   elseif args.movez then sendmovez(connection, args.movez)
   else
      connection:send("HTTP/1.0 400 OK\r\nContent-Type: application/json\r\Cache-Control: private, no-store\r\n\r\n")
      connection:send('{"error":-1, "message":"Bad request"}')
   end
end
