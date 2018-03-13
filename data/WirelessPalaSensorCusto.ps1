#Script to prepare Web files
# - Integrate customization of the application
# - GZip of resultant web files
# - and finally convert compressed web files to C++ header in PROGMEM

#List here web files specific to this project/application
$specificFiles="calib.html"

#list here files that need to be merged with Common templates
$applicationName="WirelessPalaSensor"
$shortApplicationName="WPalaSensor"
$templatesWithCustoFiles=@{
    #---Status.html---
    "status.html"=@{
        ApplicationName=$applicationName
        ;
        ShortApplicationName=$shortApplicationName
        ;
        HTMLContent=@'
        <h2 class="content-subhead">PalaSensor <span id="l2"><h6 style="display:inline"><b> Loading...</b></h6></span></h2>
        Last Home Automation HTTP Result : <span id="lhar"></span><br>
        Last Home Automation Temperature : <span id="lhat"></span><br>
        <br>
        Last ConnectionBox HTTP Result : <span id="lcr"></span><br>
        Last ConnectionBox Temperature : <span id="lct"></span><br>
        <br>
        Last Built-In Temperature : <span id="low"></span> (<span id="owu"></span>Used)<br>
        <br>
        Pushed Temperature <span id="pt"></span> (pos : <span id="p50"></span>/<span id="p5"></span>)<br>
'@
        ;
        HTMLScriptInReady=@'
        $.getJSON("/gs1", function(GS1){

            $.each(GS1,function(k,v){
                $('#'+k).html(v);
            })
            $("#l2").fadeOut();
        })
        .fail(function(){
            $("#l2").html('<h4 style="display:inline;color:red;"><b> Failed</b></h4>');
        });
'@
    }
    ;
    #---config.html---
    "config.html"=@{
        ApplicationName=$applicationName
        ;
        ShortApplicationName=$shortApplicationName
        ;
        HTMLContent=@'
        <h2 class="content-subhead">Sensor Simulator</h2>

        <div class="pure-control-group">
            <label>Calibrator</label>
            <a class="pure-button" href="/calib" style="background: rgb(28, 184, 65);">Run</a>
        </div>
        <div class="pure-control-group">
            <label for="sha">Coeff A</label>
            <input type='number' id='sha' name='sha' step="0.0000000000000001" size=30>
        </div>
        <div class="pure-control-group">
            <label for="shb">Coeff B</label>
            <input type='number' id='shb' name='shb' step="0.0000000000000001" size=30>
        </div>
        <div class="pure-control-group">
            <label for="shc">Coeff C</label>
            <input type='number' id='shc' name='shc' step="0.0000000000000001" size=30>
        </div>

        <h2 class="content-subhead">Home Automation</h2>

        <div class="pure-control-group">
            <label for="hae">Type</label>
            <select id='hae' name='hae'>
                <option value="0">None</option>
                <option value="1">Jeedom</option>
                <option value="2">Fibaro</option>
            </select>
        </div>

        <div id='ha' style='display:none'>
            <div class="pure-control-group">
                <label for="hatls">SSL/TLS</label>
                <input type='checkbox' id='hatls' name='hatls'>
            </div>
            <div class="pure-control-group">
                <label for="hah">Hostname</label>
                <input type='text' id='hah' name='hah' maxlength='64' pattern='[A-Za-z0-9-.]+' size='50' title='DNS name or IP of the Jeedom server'>
                <span class="pure-form-message-inline">(Hostname should match with certificate name if SSL/TLS is enabled)</span>
            </div>

            <div id='j' style='display:none'>

                <div class="pure-control-group">
                    <label for="ja">ApiKey</label>
                    <input type='password' id='ja' name='ja' maxlength='48' pattern='[A-Za-z0-9-.]+' size=50 title='APIKey from Jeedom configuration webpage'>
                </div>
            </div>

            <div id='fib' style='display:none'>
            
                <div class="pure-control-group">
                    <label for="fu">Username</label>
                    <input type='text' id='fu' name='fu' maxlength='64'>
                </div>
                <div class="pure-control-group">
                    <label for="fp">Password</label>
                    <input type='password' id='fp' name='fp' maxlength='64'>
                </div>
            </div>
            <div class="pure-control-group">
                <label for="hatid">Temperature Id</label>
                <input type='number' id='hatid' name='hatid' min='0' max='65535'>
                <span class="pure-form-message-inline">(Command Or Device Id)</span>
            </div>
            <div id='hatlse'>
                <div class="pure-control-group">
                    <label for="hafp">TLS FingerPrint</label>
                    <input type='text' id='hafp' name='hafp' maxlength='59' pattern='^([0-9A-Fa-f]{2}[ :-]*){19}([0-9A-Fa-f]{2})$' size='65'>
                    <span class="pure-form-message-inline">(separators are : &lt;none&gt;,&lt;space&gt;,:,-)</span>
                </div>
            </div>
        </div>

        <h2 class="content-subhead">Palazzetti</h2>
        
        <div class="pure-control-group">
            <label for="cbe" class="pure-checkbox">Palazzetti ConnectionBox</label>
            <input type='checkbox' id='cbe' name='cbe'>
        </div>
        <div id='cb' style='display:none'>
            <div class="pure-control-group">
                <label for="cbi">ConnectionBox IP</label>
                <input type='text' id='cbi' name='cbi' pattern='((^|\.)((25[0-5])|(2[0-4]\d)|(1\d\d)|([1-9]?\d))){4}$'>
            </div>
        </div>
'@
        ;
        HTMLScript=@'
        function onHAEChange(){
            switch($("#hae").val()){
                case "0":
                    $("#ha").hide();
                    break;
                case "1":
                    $("#fib").hide();
                    $("#j").show();
                    $("#ha").show();
                    break;
                case "2":
                    $("#j").hide();
                    $("#fib").show();
                    $("#ha").show();
                    break;
            }
        };
        $("#hae").change(onHAEChange);

        function onHATLSChange(){
            if($("#hatls").prop("checked")) $("#hatlse").show();
            else $("#hatlse").hide();
        };
        $("#hatls").change(onHATLSChange);

        function onCBEChange(){
            if($("#cbe").prop("checked")) $("#cb").show();
            else $("#cb").hide();
        };
        $("#cbe").change(onCBEChange);
'@
        ;
        HTMLFillinConfigForm=@'
'@
    }
    ;
    #---fw.html---
    "fw.html"=@{
        ApplicationName=$applicationName
        ;
        ShortApplicationName=$shortApplicationName
    }
    ;
    #---discover.html---
    "discover.html"=@{
        ApplicationName=$applicationName
        ;
        ShortApplicationName=$shortApplicationName
    }
}

#call script that prepare Common Web Files and contain compression/Convert/Merge functions
. ..\src\data\_prepareCommonWebFiles.ps1

$path=(Split-Path -Path $MyInvocation.MyCommand.Path)
$templatePath=($path+"\..\src\data")

Write-Host "--- Prepare Application Web Files ---"
Convert-TemplatesWithCustoToCppHeader -templatePath $templatePath -filesAndCusto $templatesWithCustoFiles -destinationPath $path
Convert-FilesToCppHeader -Path $path -FileNames $specificFiles
Write-Host ""