const express = require('express');
const path = require('path');
const { GoogleSpreadsheet } = require('google-spreadsheet');

const creds = require('./client_secret.json');
const { pid } = require('process');
const { copyFileSync } = require('fs');
const doc = new GoogleSpreadsheet(creds.sheet_key);

const app = express();
const port = creds.port;

// radius of earth in meters
var earthRadius = 6369600.0

function haversine(coords1, coords2) {
    //Convert degrees to radians  
    var lat1 = coords1[0] * Math.PI / 180.0;
    var lat2 = coords2[0] * Math.PI / 180.0;
    var lon1 = coords1[1] * Math.PI / 180.0;
    var lon2 = coords2[1] * Math.PI / 180.0;

    var dlat = lat2 - lat1; // difference in latitudes
    var dlon = lon2 - lon1; // difference in longitudes

    var a = Math.sin(dlat / 2) * Math.sin(dlat / 2) +
            Math.cos(lat1) * Math.cos(lat2) *
            Math.sin(dlon / 2) * Math.sin(dlon / 2);
    
    var c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a));
    var d = earthRadius * c;

    return d; // in meters
} // End of haversine

function calculateCenter(sheet, rowNum) {
    //Calculating center coords
    
    var x = 0.0;
    var y = 0.0;
    var z = 0.0;
    
    var centerLat;
    var centerLon;

    var coords = Array(3);
    var cartPoints = Array(3);
    for (let i = rowNum-3; i < rowNum; i++) {
        var degLat = sheet.getCell(i,2).value;
        var degLon = sheet.getCell(i,3).value;

        coords[i - rowNum + 3] = [degLat, degLon];
    }

    // make first point origin on cartesian plane, then use Haversine to calculate other points
    cartPoints[0] = [0,0];

    var xMultiplier;
    var yMultiplier;

    if (coords[1][0] < coords[0][0]) {
        yMultiplier = -1.0;
    }
    else {
        yMultiplier = 1.0;
    }

    if (coords[1][1]< coords[0][1]) {
        xMultiplier = -1.0;
    }
    else {
        xMultiplier = 1.0;
    }


    var x1 = haversine(coords[0], [coords[0][0], coords[1][1]] ) * xMultiplier; // Same lats
    var y1 = haversine(coords[0], [coords[1][0], coords[0][1]]) * yMultiplier; // Same lons

    cartPoints[1] = [x1, y1];

    if (coords[2][0] < coords[0][0]) {
        yMultiplier = -1.0;
    }
    else {
        yMultiplier = 1.0;
    }

    if (coords[2][1] < coords[0][1]) {
        xMultiplier = -1.0;
    }
    else {
        xMultiplier = 1.0;
    }
    var x2 = haversine(coords[0], [coords[0][0], coords[2][1]]) * xMultiplier; // Same lats
    var y2 = haversine(coords[0], [coords[2][0], coords[0][1]]) * yMultiplier; // Same lons
    cartPoints[2] = [x2, y2];

    var xCent = (x2*x2/(2.0*y2) - x1*x1/(2.0*y1) + y2/2.0 - y1/2.0) / (x2/y2 - x1/y1);
    var yCent = (-x1/y1) * (xCent - x1/2.0) + y1/2.0;

    //To calculate difference in angles, find vert. and horiz. distance of center from origin then use theta = d / r to calculate change in theta and add to lat/lon
    //To determine whether theta is +/-, check the direction the center is from origin
    
    var centLat = yCent / earthRadius * 180.0 / Math.PI + coords[0][0];
    var centLon = xCent / earthRadius * 180.0 / Math.PI + coords[0][1];
    
    return [centLat, centLon];
} //End of calculate center

function calculateAzimuth(currentCoords, centerCoords) {
    //Calculating azimuth:
   console.log(`Current: ${currentCoords}`);
   console.log(`Center: ${centerCoords}`);
    if (currentCoords[0] < centerCoords[0]) {
        yMultiplier = -1.0;
    }
    else {
        yMultiplier = 1.0;
    }

    if (currentCoords[1] < centerCoords[1]) {
        xMultiplier = -1.0;
    }
    else {
        xMultiplier = 1.0;
    }

    var x_azi = haversine(centerCoords, [centerCoords[0], currentCoords[1]]) * xMultiplier; // Same lats
    var y_azi = haversine(centerCoords, [currentCoords[0], centerCoords[1]]) * yMultiplier; // Same lons

    var azi = Math.atan(y_azi / x_azi) * 180.0 / Math.PI;
    if (x_azi < 0) { 
        azi -= 180.0;
    }
    azi = 90 - azi;
  
    console.log(`Azimuth: ${azi} X: ${x_azi} Y: ${y_azi}`);
    return azi;
}

async function addToSheet(request) {
    await doc.useServiceAccountAuth({
        client_email: creds.client_email,
        private_key: creds.private_key,
    });
    await doc.loadInfo();
    const sheet = doc.sheetsByIndex[0];
    
    const rowNum = sheet.rowCount;
    await sheet.loadCells(`A1:J${rowNum}`);

    
    /*
    if (haversine(coords[0], coords[1]) < 5 || haversine(coords[0], coords[2]) < 5 || haversine(coords[1], coords[2]) < 5) { //If any of the three points are within 5m of each other
        centLat = sheet.getCell(rowNum-1, 4).value;
        centLon = sheet.getCell(rowNum-1, 5).value;
    } //Keep center the same as previous
    */

    var centCoords = calculateCenter(sheet, rowNum);
    var centLat = centCoords[0];
    var centLon = centCoords[1];

    //Temporary
    centLat = 39.79514;
    centLon = -97.843487;

    centCoords = [centLat, centLon];

    var currCoords = [request.query.lat, request.query.lon];
    var azimuth = calculateAzimuth(currCoords, centCoords);

  

    await sheet.addRow({
        Datetime: request.query.dateTime, 
        UniqueID: request.query.id, 
        Latitude: request.query.lat, 
        Longitude: request.query.lon,
        CenterLat: centLat,
        CenterLon: centLon, 
        Battery: request.query.bat,
        Speed: request.query.speed,
        Alt: request.query.alt,
        Azimuth: Math.round(azimuth)});
}

async function accessSheetData(response) {
    await doc.useServiceAccountAuth({
        client_email: creds.client_email,
        private_key: creds.private_key,
    });

    await doc.loadInfo();

    const sheet = doc.sheetsByIndex[0];
   
    const rowNum = sheet.rowCount;
    await sheet.loadCells(`A${rowNum}:J${rowNum}`);
    const data = {
        datetime: sheet.getCell(rowNum-1, 0).value,
        id: sheet.getCell(rowNum-1, 1).value,
        lati: sheet.getCell(rowNum-1, 2).value, 
        long: sheet.getCell(rowNum-1, 3).value,
        center_lat: sheet.getCell(rowNum-1, 4).value,
        center_lon: sheet.getCell(rowNum-1, 5).value,
        bat: sheet.getCell(rowNum-1, 6).value,
        speed: sheet.getCell(rowNum-1, 7).value,
        alt: sheet.getCell(rowNum-1, 8).value,
        azi: sheet.getCell(rowNum-1, 9).value};
    response.send(data);
}

//accessSpreadsheet();

app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, '/index.html'));
});

app.get('/vars', (req, res) => {
    res.send('900'); // Return how many seconds for the unit to sleep
    addToSheet(req);
    console.log(req.query.dateTime);
    console.log(req.query.id);
    console.log(req.query.lat);
    console.log(req.query.lon);
    console.log(req.query.bat);
    console.log(req.query.speed);
    console.log(req.query.alt);
});

app.get('/update', (req, res) => {
    console.log("Sent data");
    //res.send(sheet.rowCount);
    accessSheetData(res);
});

app.get('/dev', (req, res) => {
    console.log("Sent data (dev mode)");
    accessSheetData(res);
});

app.listen(port, "0.0.0.0", () => {
    console.log(`Example app listening on port ${port}!`)
});
