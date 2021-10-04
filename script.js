//https://script.google.com/macros/s/AKfycbycR7N4a3pIuYFCfjR3Ys_wp7yAUb2-M6okvlhYkhzTHD6cOGaKUMcyG9MAiwltS400RQ/exec?node=test&action=getBaseInfo

function doGet(parametres) {
  try {
    // get local time
    var timestamp = now();   // UTC in unix format
    var localTime = excelLocalDate(timestamp);  // local Time in excel format
    var timezone = new Date().getTimezoneOffset()/60;

    var node = parametres.parameter.node;       // identificateur unique de la device emetrice ex : node01
    if (!node) throw 'bad device';  // doesnt use node 
    var action = parametres.parameter.action;        // nom de l'evenerment :  ex BP0
    if (!action) throw 'bad action';
  } catch (error) {
    var eventJSON = {
      'status': false,
      'message': 'Err: Invalide parameter : ' + error,
      //'timestamp' : timestamp
    }
    return ContentService.createTextOutput(JSON.stringify(eventJSON)).setMimeType(ContentService.MimeType.JSON);
  }

  // minimum de parametre valide  on enregistre l'action dans la page 'Row_Data'
  // SpreadsheetApp.getActiveSpreadsheet().getSheetByName('NAME').getRange('A1').setValue(aValue);
  var sheet = SpreadsheetApp.getActiveSpreadsheet().getSheetByName('RowData');
  var newRow = sheet.getLastRow() + 1;
  var values = [[timestamp, localTime, node, action, '']];
  sheet.getRange(newRow, 1, 1, 5).setValues(values);
  // indique la lecture de donnée
  var range = SpreadsheetApp.getActiveSpreadsheet().getRangeByName('BaseInfo');
  var values = range.getValues();
  var baseInfo = values[0][0];
  //values[1][0] = baseInfo;     // signale la version lue
  //values[1][1] = excelLocalDate(); // signale la date de lecture
  //range.setValues(values);

    var eventJSON = {
      'status': true,
      'message': 'Success',
      'action': action,
      'timestamp': timestamp,
      'timezone' : timezone,
      'baseInfo': baseInfo,
    }



if (action == 'getBaseInfo') {
    result = SpreadsheetApp.getActiveSpreadsheet().getRangeByName('BaseInfo').getValues();
    result[0][1]=unixUTC(result[0][1]);
    result[1][1]=unixUTC(result[1][1]);
    eventJSON.answer = result;
    sheet.getRange(newRow, 5).setValue('Ok');
    return ContentService.createTextOutput(JSON.stringify(eventJSON)).setMimeType(ContentService.MimeType.JSON);
  }


  if (action == 'getPlagesHoraire') {
    result = SpreadsheetApp.getActiveSpreadsheet().getRangeByName('PlagesHoraire').getValues();;
    eventJSON.answer = result;
    sheet.getRange(newRow, 5).setValue('Ok');
    return ContentService.createTextOutput(JSON.stringify(eventJSON)).setMimeType(ContentService.MimeType.JSON);
  }

  var id = parametres.parameter.ID;
  var key = parametres.parameter.KEY;

  if (action == 'badgeEnter') {
    var time = parametres.parameter.time;
    if (!time) time = localTime;
    var sheet2 = SpreadsheetApp.getActiveSpreadsheet().getSheetByName('Historique');
    var newRow2 = sheet2.getLastRow() + 1;
    var values = [[node, time, key, id, action]];
    sheet2.getRange(newRow2, 1, 1, 5).setValues(values);
    var eventJSON = {
      'status': true,
      'message': 'Success',
      'action': action,
      'timestamp': timestamp,
      'checkChange': checkChange
    }
    sheet.getRange(newRow, 5).setValue('Ok');
    sheet.getRange(newRow, 6).setValue(id + ' ' + key);
    return ContentService.createTextOutput(JSON.stringify(eventJSON)).setMimeType(ContentService.MimeType.JSON);
  }

  if (action == 'getBadgeInfo') {
    var sheet2 = SpreadsheetApp.getActiveSpreadsheet().getSheetByName('Badges');
    var lastRow2 = sheet2.getLastRow();
    var values = sheet2.getRange(2, 1, lastRow2, 6).getValues();
    var result = "";
    for (var i = 0; i < lastRow2; i++) {
      if (values[i][0] == key) {
        var result = values[i];
        result[2] = new Date(result[2]) / 1000;
        result[3] = new Date(result[3]) / 1000;
        break;
      }
    }

    if (result) {
      sheet.getRange(newRow, 5).setValue('Ok');
    } else {
      result = ['Not found'];
      sheet.getRange(newRow, 5).setValue('Not found');
    }
    
    var eventJSON = {
      'status': true,
      'message': 'Success',
      'action': action,
      'timestamp': timestamp,
      'checkChange': checkChange,
      'result': result
    }
    sheet.getRange(newRow, 6).setValue(key);

    return ContentService.createTextOutput(JSON.stringify(eventJSON)).setMimeType(ContentService.MimeType.JSON);
  }



  // default error about action


  var eventJSON = {
    'status': false,
    'message': 'err: action unknown',
    'timestamp': timestamp,
    'checkChange': checkChange,
    'param': {
      'node': node,
      'action': action
    }
  }




  return ContentService.createTextOutput(JSON.stringify(eventJSON)).setMimeType(ContentService.MimeType.JSON);
}

// trigged on every user change
function onChange(e) {
  //  Logger.log( 'on change trigged' );
  //   for( i in e )
  //    Logger.log( i );
  var range = SpreadsheetApp.getActiveSpreadsheet().getRangeByName('BaseInfo');
  var values = range.getValues();
  var checkWrite = values[0][0];
  var checkRead = values[1][0];
  if (checkWrite <= checkRead) values[0][0] = checkRead + 1;
  values[0][1] = excelLocalDate();
  range.setValues(values);
}

// conversion Unix Timestamp to Excel local date value
// a float : integer part = nombre de jour depuis 1/1/1900  
// fraction part : heure en fraction de jours
function excelLocalDate(aTimestamp) {
  if (!aTimestamp) aTimestamp = now();
  // calcul de la date a la microsoft   25569 = nombre de jours entre 1/1/1900 er 1/1/1970
  var localTime = (aTimestamp - new Date().getTimezoneOffset() * 60) / 86400 + 25569;
  return localTime;
}

// conversion Excel local date to a unix time stamp value
function unixUTC(aExeclDate) {
  //var unixUTC = Math.round((aExeclDate - 25569) * 86400) + (new Date().getTimezoneOffset() * 60);
  var unixUTC = (aExeclDate - 25569) * 86400 + (new Date().getTimezoneOffset() * 60);
  return unixUTC;
}




// timestamp unix : nombre de secondes depuis 1970
function now() {
  return Math.floor(new Date().getTime() / 1000);
}
