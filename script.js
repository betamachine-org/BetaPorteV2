function doGet(parametres) {
  try {

    var node = parametres.parameter.node;       // identificateur unique de la device emetrice ex : node01
    if (!node) throw 'bad device';  // doesnt use node 
    var action = parametres.parameter.action;        // nom de l'evenerment :  ex BP0
    if (!action) throw 'bad action';
  } catch (error) {
    var eventJSON = {
      'status': false,
      'message': 'Error ' + error,
    }
    return ContentService.createTextOutput(JSON.stringify(eventJSON)).setMimeType(ContentService.MimeType.JSON);
  }

  // minimum de parametre valide  on enregistre l'action dans la page 'Row_Data'
  // SpreadsheetApp.getActiveSpreadsheet().getSheetByName('NAME').getRange('A1').setValue(aValue);
  var date = new Date();  // javascript date 

  var sheet = SpreadsheetApp.getActiveSpreadsheet().getSheetByName('RowData');
  var newRow = sheet.getLastRow() + 1;
  var shortParam = parametres.parameter.json;
  if (shortParam && shortParam.length > 64) shortParam = shortParam.substring(1, 60) + "..."
  var values = [[date, node, action, shortParam, '??']];
  sheet.getRange(newRow, 1, 1, 5).setValues(values);
  // indique la version de la base
  var range = SpreadsheetApp.getActiveSpreadsheet().getRangeByName('BaseInfo');
  var values = range.getValues();
  var baseIndex = values[0][0];
  //values[1][0] = baseInfo;     // signale la version lue
  //values[1][1] = excelLocalDate(); // signale la date de lecture
  //range.setValues(values);

  var eventJSON = {
    'status': true,
    'message': 'Ok',
    'action': action,
  }

  // retourne le timestamp, la time zone, la base index  (same as hiso)
  if (action == 'check') {
    var timeStamp = unixLocalTime(date);   // local time in unix format
    var timeZone = date.getTimezoneOffset() / 60;

    var result = {
      'timestamp': timeStamp,
      'timezone': timeZone,
      'baseindex': baseIndex,
    }

    eventJSON.answer = result;
    sheet.getRange(newRow, 6).setValue('baseIndex=' + baseIndex + ' timeZone=' + timeZone);
    sheet.getRange(newRow, 5).setValue('Ok');
    return ContentService.createTextOutput(JSON.stringify(eventJSON)).setMimeType(ContentService.MimeType.JSON);
  }




  if (action == 'getPlagesHoraire') {
    plagesData = SpreadsheetApp.getActiveSpreadsheet().getRangeByName('PlagesHoraire').getValues();

    var plages = {};

    // transformation en named array sur les noms de la premiere collone
    // 
    for (N = 0; N < plagesData.length; N++) {
      var data = plagesData[N];
      var name = data.shift();
      plages[name] = data;
    }

    var result = {
      'baseindex': baseIndex,
      'length': Object.keys(plages).length,
      'plages': plages,
    }


    eventJSON.answer = result;
    sheet.getRange(newRow, 5).setValue('Ok');
    return ContentService.createTextOutput(JSON.stringify(eventJSON)).setMimeType(ContentService.MimeType.JSON);
  }

  if (action == 'getBadges') {
    // get max(=10) badges from first(=1)
    var paramJson = parametres.parameter.json;        // first=xxxx  max=xxxx


    var first = 1;
    var max = 10;
    // get obtional parameters
    if (paramJson) {
      var params = JSON.parse(paramJson);
      first = params.first;
      max = params.max;
      if (max < 1) max = 10;
    }

    if (max < 1) max = 1;
    if (first < 1) first = 1;

    //marque la version en cours de lecture uniquement si on est en debut de lecture
    var range = SpreadsheetApp.getActiveSpreadsheet().getRangeByName('BaseInfo');
    var values = range.getValues();
    var baseIndex = values[0][0];
    if (first == 1) {
      values[1][1] = date;
      values[1][0] = baseIndex;
      range.setValues(values);
    }


    sheetBadges = SpreadsheetApp.getActiveSpreadsheet().getSheetByName('Badges');
    totalBadges = sheetBadges.getLastRow() - 1;
    if (totalBadges > 1000) totalBadges = 1000;  // limitation a 1000 bagdes 
    var len = max;
    if (first - 1 + len > totalBadges) len = totalBadges - first + 1;

    sheet.getRange(newRow, 6).setValue(len + ' badges (' + first + '-' + (first + len - 1) + ') base index=' + baseIndex);


    var badges;
    if (len > 0) {
      badges = sheetBadges.getRange(first + 1, 1, len, 6).getValues();
      // conversion des dates en day from 2000  (make json string much shorter)
      for (N = 0; N < len; N++) {
        // todo limitation de la taille des données
        badges[N][2] = unixDaysFrom2000(badges[N][2]);
        badges[N][3] = unixDaysFrom2000(badges[N][3]);
      }
    }
    var result = {
      'baseindex': baseIndex,
      'first': first,
      'len': len,
      'total': totalBadges,
      'eof': (first - 1 + len >= totalBadges),
      'badges': badges,
    }

    eventJSON.answer = result;
    sheet.getRange(newRow, 5).setValue('Ok');
    return ContentService.createTextOutput(JSON.stringify(eventJSON)).setMimeType(ContentService.MimeType.JSON);
  }

  // param is an array of object to log type '{"action":"badge ok","info":"0EXXXXXX"}
  if (action == 'histo') {
    sheetHisto = SpreadsheetApp.getActiveSpreadsheet().getSheetByName('Historique');
    var paramJson = parametres.parameter.json;        // array 
    if (!paramJson) {
      eventJSON.status = false,
        eventJSON.message = 'Error no param info'
    } else {
      var params = JSON.parse(paramJson);
      if (!params) {
        eventJSON.status = false,
          event.JSON.message = 'Error json param'
      } else {
        for (var N = 0; N < params.liste.length; N++) {
          var action = params.liste[N].action;
          var info = params.liste[N].info;
          var timeStamp = jsDate(params.liste[N].timestamp)
          var values = [[
            timeStamp,
            node,
            action,
            info,
            'Ok'
          ]];
          sheet.getRange(newRow + N, 1, 1, 5).setValues(values); //save in RowData
          // save in histo
          var newRowHisto = sheetHisto.getLastRow() + 1;
          // correction de la date si pas de date valide
          if (values[0][0].getUTCFullYear() < 2000) values[0][0] = date;
          sheetHisto.getRange(newRowHisto, 1, 1, 5).setValues(values); //save in Histo


          // save date in badge liste for this uuid
          if (action.startsWith('badge')) {
            var aUUID = info;
            sheetBadges = SpreadsheetApp.getActiveSpreadsheet().getSheetByName('Badges');
            totalBadges = sheetBadges.getLastRow() - 1;
            badges = sheetBadges.getRange(2, 1, totalBadges, 1).getValues();
            for (var N1 = 0; N1 < totalBadges; N1++) {
              if (badges[N1][0] == aUUID) {
                sheetBadges.getRange(N1 + 2, 7).setValue(timeStamp);
                break;
              }
            }
          }
        }

        var timeStamp = unixLocalTime(date);   // local time in unix format
        var timeZone = date.getTimezoneOffset() / 60;

        var result = {
          'timestamp': timeStamp,
          'timezone': timeZone,
          'baseindex': baseIndex,
        }

        eventJSON.answer = result;

        var values = [['baseIndex=' + baseIndex + ' timeZone=' + timeZone + ' histo len=' + params.liste.length, date]];
        sheet.getRange(newRow, 6, 1, 2).setValues(values);

      }
    }
    sheet.getRange(newRow, 5).setValue(eventJSON.status ? 'Ok' : 'Err');
    return ContentService.createTextOutput(JSON.stringify(eventJSON)).setMimeType(ContentService.MimeType.JSON);
  }




  sheet.getRange(newRow, 5).setValue('Err action : ' + action);

  var eventJSON = {
    'status': false,
    'message': 'action unknown',
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
  // indique une nouvelle version
  var range = SpreadsheetApp.getActiveSpreadsheet().getRangeByName('BaseInfo');
  var values = range.getValues();
  var checkWrite = values[0][0];
  var checkRead = values[1][0];
  if (checkWrite <= checkRead) {
    // increment base index
    checkWrite = checkRead + 1;
    values[0][0] = checkWrite;
    // enregistre dans rowdata le changement de version
    var sheet = SpreadsheetApp.getActiveSpreadsheet().getSheetByName('RowData');
    var newRow = sheet.getLastRow() + 1;
    var date = new Date();  // javascript date 
    var rowValues = [[date, 'gsheet', 'baseIndexChange', 'baseIndex=' + checkWrite, 'Ok']];
    sheet.getRange(newRow, 1, 1, 5).setValues(rowValues);

  }
  values[0][1] = new Date();
  range.setValues(values);




}

// conversion Excel date to a unix time stamp value in local time
function unixLocalTime(aExeclDate) {
  if (!aExeclDate) aExeclDate = new Date();
  return Math.floor(aExeclDate.getTime() / 1000) - aExeclDate.getTimezoneOffset() * 60;
}

function jsDate(aUnixTimeStamp) {
  return new Date((aUnixTimeStamp + (new Date().getTimezoneOffset() * 60)) * 1000);
}

//Entre le 01/01/1970 et le 01/01/2000, il s'est écoulé 10957 jours soit 30 ans.


function unixDaysFrom2000(aExeclDate) {
  if (!aExeclDate) aExeclDate = new Date();
  return Math.floor((aExeclDate.getTime() - new Date(2000, 12, 1).getTime()) / (1000 * 3600 * 24));
}


// timestamp unix : nombre de secondes depuis 1970
function now() {
  return Math.floor(new Date().getTime() / 1000);
}
