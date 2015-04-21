  var xmlHttp = null;
  var refreshTimer = null;
  var temp_data;
  var temp_chart;
  var temp_options;
  
  // Load the Visualization API and the piechart package.
  google.load('visualization', '1', {'packages':['corechart']});
  // Set a callback to run when the Google Visualization API is loaded.
  google.setOnLoadCallback(createTempChart);
  // Callback that creates and populates a data table,
  // instantiates the chart, passes in the data and
  // draws it.
  function createTempChart() {

    // Create our data table.
    temp_data = new google.visualization.DataTable();
    temp_data.addColumn('string', 'id');
    temp_data.addColumn('number', 'Temperature 1');
	temp_data.addColumn('number', 'Temperature 2');
	temp_data.addColumn('number', 'Temperature Bed');	

    // Set chart options
    temp_options = {'title':'Temperatures',
                   'width':400,
                   'height':300};

    // Instantiate and draw our chart, passing in some options.
    temp_chart = new google.visualization.LineChart(document.getElementById('chart_div'));
    temp_chart.draw(temp_data, temp_options);
  }
  function updateTempChart(temp1,temp2,temp3)
  {
	//remove first row, if we have more the 100 data entries
	if(temp_data.getNumberOfRows() > 100) temp_data.removeRow(0);
	
	//add a new data entry
	temp_data.addRow(['', parseFloat(temp1),parseFloat(temp2),parseFloat(temp3)]);
	//draw chart
	temp_chart.draw(temp_data, temp_options);
  }
  
  // request temperature values 
  function getStatus()
  {
	var url = "/get?status=1";

	xmlHttp = new XMLHttpRequest();
	xmlHttp.onreadystatechange = processStatus;
	xmlHttp.open("GET", url, true);
	xmlHttp.send( null );
  }
  // handle recieved temperature values 
  function processStatus()
  {
	processRequest();
	if (xmlHttp.readyState == 4) {
		var data = JSON.parse(xmlHttp.responseText);
		//temperatures
		document.getElementById("measure-value-temp1a").innerHTML = data.Temp1 + "&deg;C";
		document.getElementById("measure-value-temp1a").style.width = ((data.Temp1/300)*100)+"%";
		document.getElementById("target-value-temp1a").style.width = ((data.Temp1Target/300)*100)+"%";
		//temperatures
		document.getElementById("measure-value-temp1b").innerHTML = data.Temp1 + "&deg;C";
		document.getElementById("measure-value-temp1b").style.width = ((data.Temp1/300)*100)+"%";
		document.getElementById("target-value-temp1b").style.width = ((data.Temp1Target/300)*100)+"%";
		if(data.Temp2 != "--")
		{
			document.getElementById("measure-value-temp2a").innerHTML = data.Temp2 + "&deg;C";
			document.getElementById("measure-value-temp2a").style.width = ((data.Temp2/300)*100)+"%";
			document.getElementById("target-value-temp2a").style.width = ((data.Temp2Target/300)*100)+"%";
			document.getElementById("hotend2a").style.display = 'block';
			document.getElementById("measure-value-temp2b").innerHTML = data.Temp2 + "&deg;C";
			document.getElementById("measure-value-temp2b").style.width = ((data.Temp2/300)*100)+"%";
			document.getElementById("target-value-temp2b").style.width = ((data.Temp2Target/300)*100)+"%";
			document.getElementById("hotend2b").style.display = 'block';
		}
		else 
		{
			document.getElementById("hotend2a").style.display = 'none';
			document.getElementById("hotend2b").style.display = 'none';
		}
		document.getElementById("measure-value-beda").innerHTML = data.Bed + "&deg;C";
		document.getElementById("measure-value-beda").style.width = ((data.Bed/300)*100)+"%";
		document.getElementById("target-value-beda").style.width = ((data.BedTarget/300)*100)+"%";
		document.getElementById("measure-value-bedb").innerHTML = data.Bed + "&deg;C";
		document.getElementById("measure-value-bedb").style.width = ((data.Bed/300)*100)+"%";
		document.getElementById("target-value-bedb").style.width = ((data.BedTarget/300)*100)+"%";
		// Position
		document.getElementById("xPos").innerHTML = data.xPos;
		document.getElementById("yPos").innerHTML = data.yPos;
		document.getElementById("zPos").innerHTML = data.zPos;
		// print percentage
		document.getElementById("print_percent").innerHTML = data.SDpercent;
		if(data.SDselected == "yes")
		{
			if(data.SDpercent != "---")
			{
				//activate pause btn
				document.getElementById("pause_btn").style.display = 'block';
				document.getElementById("resume_btn").style.display = 'none';
			}
			else
			{
				//activate resume btn
				document.getElementById("pause_btn").style.display = 'none';
				document.getElementById("resume_btn").style.display = 'block';
			}
			//activate stop btn
			document.getElementById("stop_btn").style.display = 'block';
			document.getElementById("print_btn").style.display = 'none';
		}
		else
		{
			//activate print btn
			document.getElementById("print_btn").style.display = 'block';
			document.getElementById("pause_btn").style.display = 'none';
			document.getElementById("resume_btn").style.display = 'none';
			document.getElementById("stop_btn").style.display = 'none';
		}
		// print time			
		document.getElementById("print_time").innerHTML = data.printTime;
		
		//update chart 
		if(data.Temp2 != "--") updateTempChart(data.Temp1,data.Temp2,data.Bed);
		else updateTempChart(data.Temp1,0,data.Bed);
	}
  }
  // set new target temperatures, updates temperature values 
  function settemp(heater)
  {
	var url = "/set?heater=" + heater +"&temp=";
	var value =0;
	if(heater == 1) value = document.getElementById("temp1").value;
	else if(heater==2) value = document.getElementById("temp2").value;
	else if(heater==3) value = document.getElementById("bed").value;
	
	url = url+value;

	xmlHttp = new XMLHttpRequest();
	xmlHttp.onreadystatechange = processRequest;
	xmlHttp.open("GET", url, true);
	xmlHttp.send( null );
	
	getTemp();
  }
  // move any axis by an amount
  function move(amount,axis,speed)
  {
	 var url = "/set?move=" + amount + "&axis="+axis+"&speed="+speed;

	 xmlHttp = new XMLHttpRequest();
	 xmlHttp.onreadystatechange = processRequest;
	 xmlHttp.open("GET", url, true);
	 xmlHttp.send( null );
  }
  function move_axis(amount,axis)
  {
	 move(amount,axis,document.getElementById("move_speed").value);
  }
  //move extruder
  function extrude()
  {
	move(document.getElementById("extrude_amount").value,4,document.getElementById("extrude_speed").value);
  }
  function retract()
  {
	move(document.getElementById("retract_amount").value*-1,4,document.getElementById("retract_speed").value);
  }
  // home axes, 0 is all axis 
  function home(axis)
  {
	var url = "/set?home=" + axis;

	xmlHttp = new XMLHttpRequest();
	xmlHttp.onreadystatechange = processRequest;
	xmlHttp.open("GET", url, true);
	xmlHttp.send( null );
  }
  // Generic request processing - updates the indicator 
  function processRequest()
  {
	 if(xmlHttp.readyState == 0)
	 {
		document.getElementById("status").innerHTML = 'Initalizing...';
		document.getElementById("status").className = "initalizing";
	 }
	 else if(xmlHttp.readyState == 1)
	 {
		document.getElementById("status").innerHTML = 'Server connection established.';
		document.getElementById("status").className = "connection";
	 }
	 else if(xmlHttp.readyState == 2)
	 {
		document.getElementById("status").innerHTML = 'Request received.';
		document.getElementById("status").className = "received";
	 }
	 else if(xmlHttp.readyState == 3)
	 {
		document.getElementById("status").innerHTML = 'Processing request.';
		document.getElementById("status").className = "processing";
	 }
	 else if(xmlHttp.readyState == 4)
	 {
		if(xmlHttp.status == 200)
		{
		   document.getElementById("status").innerHTML = "";
		   document.getElementById("status").className = "ok";
		   sleep(300);
		   document.getElementById("status").className = "start";
		}
		else if(xmlHttp.status == 400)
		{
		   document.getElementById("status").innerHTML = 'Bad request.';
		   document.getElementById("status").className = "bad";
		}
	 }
  }
  // Sleep
  function sleep(milliseconds){
	 var start = new Date().getTime();
	 for (var i = 0; i < 1e7; i++)
	 {
		if ((new Date().getTime() - start) > milliseconds)
		{
		   break;
		}
	 }
  }
  // only allows number keys 
  function isNumberKey(evt){
	var charCode = (evt.which) ? evt.which : evt.keyCode;
	if (charCode == 8 || charCode == 46 || charCode == 37 || charCode == 39) 
		return true;
	if (charCode > 31 && (charCode < 48 || charCode > 57))
		return false;
	return true;
  }
  // update temperature every 5 seconds 
  function autorefresh()
  {	
	getStatus();
	refreshTimer = setInterval(function () {getStatus()},3000);
  }
		
  function uploadFile()
  {
	// unser File Objekt
	var file = document.getElementById("fileA").files[0];
	
	if(!file)
	{
		alert("No file selected");
		return;
	}	
	//FormData Objekt erzeugen
	var formData = new FormData();
	//XMLHttpRequest Objekt erzeugen
	xmlHttp = new XMLHttpRequest();
	//Fügt dem formData Objekt unser File Objekt hinzu
	formData.append("file",file);
	
	xmlHttp.onreadystatechange = function(e) {
		if(xmlHttp.readyState == 4)
		{
			alert(xmlHttp.responseText);
		}
	};
	
	xmlHttp.upload.onprogress = function(e) {
		var p = Math.round(100 / e.total * e.loaded);
		document.getElementById("file_progress").value = p;            
		document.getElementById("file_prozent").innerHTML = p + "%";
	};
	
	xmlHttp.open("POST", "/upload");
	xmlHttp.send(formData);
  }
  function refreshSD()
  {
	var url = "/getLong?files=1";
	xmlHttp = new XMLHttpRequest();
	xmlHttp.onreadystatechange = refreshSDready;
	xmlHttp.open("GET", url, true);
	xmlHttp.send( null );
  }
  function refreshSDready()
  {
	processRequest();
	if (xmlHttp.readyState == 4)
	{
		var data = JSON.parse(xmlHttp.responseText);
		if(data.SD != "ok")
		{
			document.getElementById("FileDisplay").options.length = 1;
			document.getElementById("FileDisplay").options[0].text = data.SD;
			document.getElementById("FileDisplay2").options[0].text = data.SD;
		}
		else
		{
			document.getElementById("FileDisplay").options.length = data.Files.length;
			document.getElementById("FileDisplay2").options.length = data.Files.length;
			for (var i = 0; i < data.Files.length; i++) 
			{
				document.getElementById("FileDisplay").options[i].text = data.Files[i];
				document.getElementById("FileDisplay2").options[i].text = data.Files[i];
			}				
		}
	}
  }
  function deleteFile()
  {
	if(document.getElementById("FileDisplay").selectedIndex == -1)
	{
		alert("No file selected");
		return;
	}
	var url = "/set?delete=" + document.getElementById("FileDisplay").options[document.getElementById("FileDisplay").selectedIndex].text;
	xmlHttp = new XMLHttpRequest();
	xmlHttp.onreadystatechange = deleteFileReady;
	xmlHttp.open("GET", url, true);
	xmlHttp.send( null );
  }
  function deleteFileReady()
  {
	processRequest();
	if (xmlHttp.readyState == 4)
	{
		refreshSD();
	}
  }
  function printFile()
  {
	if(document.getElementById("FileDisplay2").selectedIndex == -1)
	{
		alert("No file selected");
		return;
	}
	
	var url = "/set?print=" + document.getElementById("FileDisplay2").options[document.getElementById("FileDisplay2").selectedIndex].text;
	xmlHttp = new XMLHttpRequest();
	xmlHttp.onreadystatechange = processRequest;
	xmlHttp.open("GET", url, true);
	xmlHttp.send( null );
  }
  function pausePrint()
  {
  	var url = "/set?pause=1";
	xmlHttp = new XMLHttpRequest();
	xmlHttp.onreadystatechange = processRequest;
	xmlHttp.open("GET", url, true);
	xmlHttp.send( null );
  }
  function resumePrint()
  {
    var url = "/set?resume=1";
	xmlHttp = new XMLHttpRequest();
	xmlHttp.onreadystatechange = processRequest;
	xmlHttp.open("GET", url, true);
	xmlHttp.send( null );
  }
  function stopPrint()
  {
    var url = "/set?stop=1";
	xmlHttp = new XMLHttpRequest();
	xmlHttp.onreadystatechange = processRequest;
	xmlHttp.open("GET", url, true);
	xmlHttp.send( null );
  }
   // reboot
  function reboot()
  {
	var url = "/reboot?reboot=1";

	xmlHttp = new XMLHttpRequest();
	xmlHttp.open("GET", url, true);
	xmlHttp.send( null );
  }