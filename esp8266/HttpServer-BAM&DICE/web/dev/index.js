  var xmlHttp = null;
  var refreshTimer = null;
  var temp_chart;
  var chart_created=false;
  var config_changed=false;
  var temp1Target_changed=false;
  var temp2Target_changed=false;
  var temp3Target_changed=false;
  
  function createTempChart(temp1,temp1Target,temp2,temp2Target,temp3,temp3Target) 
  {
	Chart.defaults.global.animation=false;
	Chart.defaults.global.scaleLineColor ="rgba(255,255,255,0.5)";
	Chart.defaults.global.scaleFontColor = "rgba(255,255,255,0.5)";
  
	var ctx = document.getElementById("chart").getContext("2d");
	
	// only create 2 data series
	var temp_data1;
	if(temp2 == "--")
	{
		temp_data1 = {
			labels: [""],
			datasets: [ {
				label: "Hotend 1",
				fillColor: "#444",
				strokeColor: "rgba(255,0,0,1)",
				pointColor: "rgba(255,0,0,1)",
				pointStrokeColor: "#fff",
				pointHighlightFill: "#fff",
				pointHighlightStroke: "rgba(255,0,0,1)",
				data: [temp1]
				},
				{
				label: "Hotend 1 Target",
				fillColor: "#444",
				strokeColor: "rgba(255,0,0,0.5)",
				pointColor: "rgba(255,0,0,0.5)",
				pointStrokeColor: "#fff",
				pointHighlightFill: "#fff",
				pointHighlightStroke: "rgba(255,0,0,0.5)",
				data: [temp1Target]
				},
				{
				label: "Print Bed",
				fillColor: "#444",
				strokeColor: "rgba(0,255,0,1)",
				pointColor: "rgba(0,255,0,1)",
				pointStrokeColor: "#fff",
				pointHighlightFill: "#fff",
				pointHighlightStroke: "rgba(0,255,0,1)",
				data: [temp3]
				},
				{
				label: "Print Bed Target",
				fillColor: "#444",
				strokeColor: "rgba(0,255,0,0.5)",
				pointColor: "rgba(0,255,0,0.5)",
				pointStrokeColor: "#fff",
				pointHighlightFill: "#fff",
				pointHighlightStroke: "rgba(0,255,0,0.5)",
				data: [temp3Target]
				}
				]
			};
	}
	else   //create 3 Data series
	{
		temp_data1 = {
			labels: [""],
			datasets: [ {
				label: "Hotend 1",
				fillColor: "#444",
				strokeColor: "rgba(255,0,0,1)",
				pointColor: "rgba(255,0,0,1)",
				pointStrokeColor: "#fff",
				pointHighlightFill: "#fff",
				pointHighlightStroke: "rgba(255,0,0,1)",
				data: [temp1]
				},
				{
				label: "Hotend 1 Target",
				fillColor: "#444",
				strokeColor: "rgba(255,0,0,0.5)",
				pointColor: "rgba(255,0,0,0.5)",
				pointStrokeColor: "#fff",
				pointHighlightFill: "#fff",
				pointHighlightStroke: "rgba(255,0,0,0.5)",
				data: [temp1Target]
				},
				{
				label: "Hotend 2",
				fillColor: "#444",
				strokeColor: "rgba(0,255,0,1)",
				pointColor: "rgba(0,255,0,1)",
				pointStrokeColor: "#fff",
				pointHighlightFill: "#fff",
				pointHighlightStroke: "rgba(0,255,0,1)",
				data: [temp2]
				},
				{
				label: "Hotend 2 Target",
				fillColor: "#444",
				strokeColor: "rgba(0,255,0,0.5)",
				pointColor: "rgba(0,255,0,0.5)",
				pointStrokeColor: "#fff",
				pointHighlightFill: "#fff",
				pointHighlightStroke: "rgba(0,255,0,0.5)",
				data: [temp2Target]
				},
				{
				label: "Print Bed",
				fillColor: "#444",
				strokeColor: "rgba(0,0,255,1)",
				pointColor: "rgba(0,0,255,1)",
				pointStrokeColor: "#fff",
				pointHighlightFill: "#fff",
				pointHighlightStroke: "rgba(0,0,255,1)",
				data: [temp3]
				},
				{
				label: "Print Bed Target",
				fillColor: "#444",
				strokeColor: "rgba(0,0,255,0.5)",
				pointColor: "rgba(0,0,255,0.5)",
				pointStrokeColor: "#fff",
				pointHighlightFill: "#fff",
				pointHighlightStroke: "rgba(0,0,255,0.5)",
				data: [temp3Target]
				}
				]
			};
	}

	var chart_options = {
		bezierCurve  : true,
		scaleShowGridLines : true, 
		pointDot : false,
		datasetFill:false, 
		scaleGridLineColor : "rgba(255,255,255,0.5)",
		multiTooltipTemplate : "<%= value %> °C",
		tooltipEvents: [],
		tooltipFillColor: "rgba(0,0,0,0)",
		onAnimationComplete: function()
		{
			var pointsArray = [];
			pointsArray.push(this.datasets[0].points[this.datasets[0].points.length-1]);
						
			this.showTooltip(pointsArray, true);
		},
		customTooltips: function(tooltip) {
			if(tooltip)
			{
				var html;
				html = '<span style="color:' + tooltip.legendColors[0].fill + ';font-size:'+tooltip.fontSize+'px">Hotend 1: ' +tooltip.labels[0] + ' (' + tooltip.labels[1] + ')<br/></span>';
				if(tooltip.labels.length > 4) 
				{
					html += '<span style="color:' + tooltip.legendColors[2].fill + ';font-size:'+tooltip.fontSize+'px">Hotend 2: ' +tooltip.labels[2] + ' (' + tooltip.labels[3] + ') <br/></span>';
					html += '<span style="color:' + tooltip.legendColors[4].fill + ';font-size:'+tooltip.fontSize+'px">Print Bed: ' +tooltip.labels[4] + ' (' + tooltip.labels[5] + ')<br/></span>';
				}
				else
					html += '<span style="color:' + tooltip.legendColors[2].fill + ';font-size:'+tooltip.fontSize+'px">Print Bed: ' +tooltip.labels[2] + ' (' + tooltip.labels[3] + ')<br/></span>';
			
				
				document.getElementById('chart_legend').innerHTML =html;
			}
		}
	}
	
	temp_chart = new Chart(ctx).Line(temp_data1, chart_options);
	
  }
  function updateTempChart(temp1,temp1Target,temp2,temp2Target,temp3,temp3Target)
  {
	if(chart_created == false)
	{
		createTempChart(temp1,temp1Target,temp2,temp2Target,temp3,temp3Target);
		chart_created = true;
	}
	else
	{
		if(temp_chart.datasets[0].points.length > 20) temp_chart.removeData();

		if(temp2 == "--")
		{
			temp_chart.addData([temp1,temp1Target,temp3,temp3Target],"");
		}
		else
		{
			temp_chart.addData([temp1,temp1Target,temp2,temp2Target,temp3,temp3Target],"");
		}		
		temp_chart.update();
	}
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
		
		if(data.error==1) return;
		
		//temperatures
		document.getElementById("temp1_value").innerHTML = data.Temp1 + "&deg;C";
		document.getElementById("temp1_bar").style.height = ((data.Temp1/300)*100)+"%";
		document.getElementById("temp1_goal").style.bottom = ((data.Temp1Target/300)*100)+"%";
		if(data.Temp2 != "--")
		{
			document.getElementById("temp2_value").innerHTML = data.Temp2 + "&deg;C";
			document.getElementById("temp2_bar").style.height = ((data.Temp2/300)*100)+"%";
			document.getElementById("temp2_goal").style.bottom = ((data.Temp2Target/300)*100)+"%";
			document.getElementById("hotend2").style.display = 'block';
		}
		else document.getElementById("hotend2").style.display = 'none';
		document.getElementById("bed_value").innerHTML = data.Bed + "&deg;C";
		document.getElementById("bed_bar").style.height = ((data.Bed/300)*100)+"%";
		document.getElementById("bed_goal").style.bottom = ((data.BedTarget/300)*100)+"%";
		
		if(temp1Target_changed == false) document.getElementById("temp1").value = data.Temp1Target;
		if(temp2Target_changed == false) document.getElementById("temp2").value = data.Temp2Target;
		if(temp3Target_changed == false) document.getElementById("bed").value = data.BedTarget;
	
		// Position
		document.getElementById("cur_position").innerHTML = "X "+data.xPos+" Y "+data.yPos+" Z "+data.zPos;
		// print percentage
		document.getElementById("cur_progress").innerHTML =  data.SDpercent+" %";
		// print time		
		var time =  data.printTime/ 60000;
		var hours = time / 60;
		var minutes = time % 60;
		document.getElementById("cur_time").innerHTML = "";
 		if(hours < 10) document.getElementById("cur_time").innerHTML += "0"; 
		document.getElementById("cur_time").innerHTML += Math.floor(hours) + ":"; 
		if(minutes < 10) document.getElementById("cur_time").innerHTML += "0";
		document.getElementById("cur_time").innerHTML += Math.floor(minutes) + " h"; 
		if(data.SDselected == "yes")
		{
			if(data.SDpercent != "---")
			{
				//activate pause btn
				document.getElementById("pause_btn").style.display = 'inherit';
				document.getElementById("resume_btn").style.display = 'none';
			}
			else
			{
				//activate resume btn
				document.getElementById("pause_btn").style.display = 'none';
				document.getElementById("resume_btn").style.display = 'inherit';
			}
			//activate stop btn
			document.getElementById("stop_btn").style.display = 'inherit';
			document.getElementById("print_btn").style.display = 'none';
		}
		else
		{
			//activate print btn
			document.getElementById("print_btn").style.display = 'inherit';
			document.getElementById("pause_btn").style.display = 'none';
			document.getElementById("resume_btn").style.display = 'none';
			document.getElementById("stop_btn").style.display = 'none';
		}
		//update chart 
		updateTempChart(data.Temp1,data.Temp1Target,data.Temp2,data.Temp2Target,data.Bed,data.BedTarget);
		
		//update wifi config if it was not changed by user
		if(config_changed == false)
		{
			document.getElementById("wifi_ssid").value = data.SSID;
			if(data.MODE == "STATION") document.getElementById("wifi_mode").selectedIndex = 0;
			else document.getElementById("wifi_mode").selectedIndex = 1;
		
			if(data.SEC == "OPEN") document.getElementById("wifi_sec").selectedIndex = 0;
			else if(data.SEC == "WEP") document.getElementById("wifi_sec").selectedIndex = 1;
			else if(data.SEC == "WPA_PSK") document.getElementById("wifi_sec").selectedIndex = 2;
			else if(data.SEC == "WPA2_PSK") document.getElementById("wifi_sec").selectedIndex = 3;
			else if(data.SEC == "WPA_WPA2_PSK") document.getElementById("wifi_sec").selectedIndex = 4;
		}
		
		// update file list
		if(data.SDinserted)
		{
			// build array of files to display
			var files =new Array();
			for (var i = 0; i < data.Files.length; i++) 
			{
				if(data.Files[i].indexOf('.') != -1)
				{
					files.push(data.Files[i]);
				}
			}	
			
			document.getElementById("FileDisplay").options.length = files.length;
			document.getElementById("FileDisplay2").options.length = files.length;
			
			for (var i = 0; i < files.length; i++) 
			{
				document.getElementById("FileDisplay").options[i].text = files[i];
				document.getElementById("FileDisplay2").options[i].text = files[i];
			}				
		}
		else
		{
			document.getElementById("FileDisplay").options.length = 1;
			document.getElementById("FileDisplay2").options.length = 0;
			document.getElementById("FileDisplay").options[0].text = "No SD card";
		}	

		//fan speed
		document.getElementById("fanSpeedVal").innerHTML = data.fanSpeed;
	}
  }
  // set new target temperatures, updates temperature values 
  function settemp(heater)
  {
	var url = "/set?heater=" + heater +"&temp=";
	var value =0;
	if(heater == 1)
	{
		value = document.getElementById("temp1").value;
		temp1Target_changed=false;
	}
	else if(heater==2)
	{	
		value = document.getElementById("temp2").value;
		temp2Target_changed=false;
	}
	else if(heater==3)
	{
		value = document.getElementById("bed").value;
		temp3Target_changed=false;
	}
	url = url+value;

	xmlHttp = new XMLHttpRequest();
	xmlHttp.onreadystatechange = processRequest;
	xmlHttp.open("GET", url, true);
	xmlHttp.send( null );
  }
  function setFanSpeed()
  {
	var url = "/set?fan=" +  document.getElementById("fanSpeed").value;
	xmlHttp = new XMLHttpRequest();
	xmlHttp.onreadystatechange = processRequest;
	xmlHttp.open("GET", url, true);
	xmlHttp.send( null );
	
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
  function gcode()
  {
	var code = document.getElementById("gcode").value;
	if(code.length > 0) sendGcode(code);	
  }
  function sendGcode(code)
  {
	var url = "/set?gcode=" + encodeURIComponent(code);
	xmlHttp = new XMLHttpRequest();
	xmlHttp.onreadystatechange = processRequest;
	xmlHttp.open("GET", url, true);
	xmlHttp.send( null );
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
		   var data = JSON.parse(xmlHttp.responseText);
		   document.getElementById("status").innerHTML = "";
		   document.getElementById("status").className = "ok";
		   document.getElementById("status_text").innerHTML = data.message;
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
  // update status every x seconds
  function autorefresh()
  {	
	getStatus();
	refreshTimer = setInterval(function () {getStatus()},3000);
  }
  // uploads a file
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
	formData.append("file",file);
	
	xmlHttp.onreadystatechange = function(e) {
		if(xmlHttp.readyState == 4)
		{
			var data = JSON.parse(xmlHttp.responseText);
			document.getElementById("status_text").innerHTML = data.message;
		}
	};
	
	xmlHttp.upload.onprogress = function(e) {
		var p = Math.round(100 / e.total * e.loaded);
		if(e.total == 0 || e.loaded == 0) p=0;
		document.getElementById("file_progress").value = p;            
		document.getElementById("file_prozent").innerHTML = p + "%";
	};
	xmlHttp.open("POST", "/upload",true);
	
	xmlHttp.send(formData);
  }
  function deleteFile()
  {
	if(document.getElementById("FileDisplay").selectedIndex == -1)
	{
		alert("No file selected");
		return;
	}
	var url = "/set?delete=" + encodeURIComponent(document.getElementById("FileDisplay").options[document.getElementById("FileDisplay").selectedIndex].text);
	xmlHttp = new XMLHttpRequest();
	xmlHttp.onreadystatechange = processRequest;
	xmlHttp.open("GET", url, true);
	xmlHttp.send( null );
  }
  function printFile()
  {
	if(document.getElementById("FileDisplay2").selectedIndex == -1)
	{
		alert("No file selected");
		return;
	}
	
	var url = "/set?print=" + encodeURIComponent(document.getElementById("FileDisplay2").options[document.getElementById("FileDisplay2").selectedIndex].text);
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
	if (confirm('Are you sure you want to reboot ?')) {
    	var url = "/setinternal?reboot=1";

		xmlHttp = new XMLHttpRequest();
		xmlHttp.open("GET", url, true);
		xmlHttp.send( null );
	}
  }
  function configChanged()
  {
	config_changed = true;
  }
  function saveConfig()
  {
	var url = "/setinternal?network=";
	url = url + document.getElementById("wifi_ssid").value;
	url = url+ "&pwd=";
	url = url + document.getElementById("wifi_pwd").value;
	url = url+ "&mode=";
	url = url+ document.getElementById("wifi_mode").options[document.getElementById("wifi_mode").selectedIndex].text;
	url = url+ "&sec=";
	url = url+ document.getElementById("wifi_sec").options[document.getElementById("wifi_sec").selectedIndex].text;
	
	xmlHttp = new XMLHttpRequest();
	xmlHttp.onreadystatechange = processRequest;
	xmlHttp.open("GET", url, true);
	xmlHttp.send( null );
	
	config_changed = false;
  }