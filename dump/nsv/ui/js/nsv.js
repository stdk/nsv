$.ajaxSetup({ cache: false })

var datatablesRuLang = {
    "sProcessing":   "Подождите...",
    "sLengthMenu":   "Показать _MENU_ записей",
    "sZeroRecords":  "Записи отсутствуют.",
    "sInfo":         "Записи с _START_ до _END_ из _TOTAL_ записей",
    "sInfoEmpty":    "Записи с 0 до 0 из 0 записей",
    "sInfoFiltered": "(отфильтровано из _MAX_ записей)",
    "sInfoPostFix":  "",
    "sSearch":       "Поиск:",
    "sUrl":          "",
    "oPaginate": {
        "sFirst": "Первая",
        "sPrevious": "Предыдущая",
        "sNext": "Следующая",
        "sLast": "Последняя"
    }
}

DEV_URL = 'devices'
CMD_URL = 'cmd'
DEV_ID = '#dev_data'
TABS_ID = '#tabs'
DATEPICKER_ID = '#datepicker'

function addTab(tab_id,name,content)
{
	$(TABS_ID+' ul').append('<li><a href="#'+tab_id+'">'+name+'</a></li>')
	$(TABS_ID).append('<div id="'+tab_id+'">'+content+'</div>')
}

function initDateTime(d)
{ 
	$(DATEPICKER_ID).datepicker("setDate",d)
	$("#hours").val(d.getHours())
	$("#minutes").val(d.getMinutes())	
}

function sendCommand(command_name,args,success_callback)
{
	$.ajax({
		url: CMD_URL+'?name=' + command_name,
		type: 'POST',
		contentType:'application/x-www-form-urlencoded',
		data: args,
		success: success_callback
	})
}

function initPage()
{
	$(TABS_ID).tabs()

	var x =$(DATEPICKER_ID).datepicker( { 
		dateFormat: 'yy-mm-dd',
	})

	$("#button-add-adbk").click(function() {
		var addr = $("#adbk-addr").val()
		sendCommand('add-adbk',{ addr : addr },function(data) {
			alert(data)
		})
	})

	$.get("version",function(data) {
		$("#tab-version").html('<h3>'+data+'</h3>');
	})

	$("#refresh-datetime").click(function() { 
		initDateTime(new Date())
	})

	$("#set-datetime").click(function() {
		var date = $(DATEPICKER_ID).datepicker('getDate')
	
		var arg = ''
		arg += (date.getMonth()+1).toString().padding(-2,'0')
		arg += date.getDate().toString().padding(-2,'0')
		arg += $("#hours").val().padding(-2,'0')
		arg += $("#minutes").val().padding(-2,'0')
		arg += (date.getYear()).toString()
		
		alert(arg)

		sendCommand('set-time',{time:arg},function(data) {
			alert(data)
		})

	})

	var iframe = '<iframe style="width:100%;height:5em" scrolling="no" frameborder="0" src="http://'+location.hostname+':3389">error</iframe>'
	$("#tab-upload").append(iframe)
}

function initData(update_interval)
{
    function drawCallback() {
      $.get("info",function(data) {
          $('#info').html(data)
        })

      $("#dev-data tr td:first-child").each(function(i) {
        var element = $(this)
        if(element.data('status') == undefined) {
          var status = element.text()
          element.data('status',status == '1')
          element.text( '●' )
          element.addClass( 'status' + status  )
        }
      })
    }
    
    var table = $('#dev-data').dataTable({
            oLanguage: datatablesRuLang,
            bPaginate: false,
            bLengthChange: false,
            iDisplayLength: 20,
            bFilter: true,
            bSort: true,
            bInfo: false,
            bAutoWidth: false,
            bProcessing: true,
            sAjaxSource: '/dev-data',
            fnDrawCallback: drawCallback
    });

    $("#dev-data tr td:first-child").live('click',function() {
      var element = $(this)
				if(element.data('status')==false || (event.ctrlKey && event.altKey) ) {
					var sn = element.next().next().text()
					if(confirm('Вы действительно хотите удалить устройство с серийным номером ' + sn + '?')) {
						sendCommand('remove-device',{sn:sn},function(data) {
							alert(data)
						})
					}
				}      
    })

    setInterval(function() {
        table.fnReloadAjax()
    },update_interval)
}

$(function() {
	initPage()
	initData(4000)
})

