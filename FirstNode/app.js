const sqlite3 = require('sqlite3').verbose();
const net = require('net');
const express = require('express');
const exphbs = require('express-handlebars');
const open = require('sqlite').open;
const { write } = require('fs');

let db;

// this is a top-level await 
(async () => {
	// open the database
	db = await open({
		filename: './db/sensors.db',
		driver: sqlite3.Database
	})
	//await db.exec('DROP TABLE devices');await db.exec('DROP TABLE devReadout');await db.exec('DROP TABLE rules');
	//db.exec('DROP TABLE devReadout');
	await db.exec('CREATE TABLE IF NOT EXISTS devices (espId, devName, devType, devUnits, CONSTRAINT pkDevices PRIMARY KEY(espId ,devName) ON CONFLICT IGNORE)');
	await db.exec('CREATE TABLE IF NOT EXISTS devReadout (espId ,devName, readOut REAL, date datetime DEFAULT current_timestamp, CONSTRAINT fkDevR__Dev FOREIGN KEY(espId, devName) REFERENCES devices(espId, devName) ON DELETE CASCADE )');
	await db.exec('CREATE TABLE IF NOT EXISTS rules (ruleName, ruleTargetId, ruleTargetName, ruleSourceId, ruleSourceName, ruleFunction, val REAL,result NUMBER, CONSTRAINT pkRules PRIMARY KEY(ruleName) ON CONFLICT IGNORE,  CONSTRAINT fkDevRT_Dev FOREIGN KEY(ruleSourceId, ruleSourceName) REFERENCES devices(espId, devName) ON DELETE CASCADE,  CONSTRAINT fkDevRS_Dev FOREIGN KEY(ruleTargetId, ruleTargetName) REFERENCES devices(espId, devName) ON DELETE CASCADE)');
})()


class Client {
	socket;
	obj;
	constructor(socket) {
		this.socket = socket;
	}
}

const app = express();

app.use(express.json());
app.use(express.urlencoded({ extended: false }));

app.use(express.static('public'));

app.engine('hbs', exphbs.engine({ extname: '.hbs' }));
app.set('view engine', 'hbs');


const server = net.createServer();
let connectedClients = []

app.get('', async (req, res) => {
	try {
		const rowsDev = await db.all('SELECT * FROM devices');
		const rowsRules = await db.all('SELECT * FROM rules');
		res.render('home', { rowsDev, rowsRules });
	} catch (error) {
		res.status(500).json({ msg: 'Błąd bazy danych', error });
	}
});

app.post('/rules', async (req, res) => {
	try {
		const sdata = req.body.sourceData.split(" "); 
		const tdata = req.body.targetData.split(" "); 
		const result = ((req.body.resData === "On") ? 1 : 0); 
		await db.run('INSERT INTO rules(ruleName, ruleTargetId, ruleTargetName, ruleSourceId, ruleSourceName, ruleFunction, val,result) VALUES(?,?,?,?,?,?,?,?)',
			[req.body.ruleName, tdata[0],tdata[1],sdata[0],sdata[1],req.body.funcData,parseFloat(req.body.val),result]);
		res.redirect('/');
	} catch (error) {
		res.status(500).json({ msg: 'Błąd bazy danych', error });
	}
});

app.post('/rules_delete', async (req, res) => {

	try {
		await db.run('DELETE FROM rules WHERE ruleName = ?',[req.query.ruleName]);
		res.redirect('/');
	} catch (error) {
		res.status(500).json({ msg: 'Błąd bazy danych', error });
	}
});

app.get('/chart', async (req, res) => {
	try {
		const rowsDev = await db.all('SELECT * FROM devices');
		const rowsRules = await db.all('SELECT * FROM rules');
		const params = req.query;
		res.render('charts', { rowsDev, rowsRules, params });
	} catch (error) {
		res.status(500).json({ msg: 'Błąd bazy danych', error });
	}
});

app.get('/chart_data', async (req, res) => {
	const rows = await db.all('SELECT readOut AS y, date x FROM devReadout WHERE espId = ? AND devName = ? ORDER BY date', [req.query.espId, req.query.devName]);
	//console.log(rows);
	res.json(rows);
});

app.get('/rules', async (req, res) => {
	try {
		const rowsDev = await db.all('SELECT * FROM devices');
		const rowsRules = await db.all('SELECT * FROM rules');
		const rule = await db.get('SELECT * FROM rules WHERE ruleName = ?', [req.query.ruleName]);
		res.render('rules', { rowsDev, rowsRules, rule });
	} catch (error) {
 		res.status(500).json({ msg: 'Błąd bazy danych', error });
	}
});

app.get('/rules_new', async (req, res) => {
	try {
		const rowsDev = await db.all('SELECT * FROM devices');
		const rowsRules = await db.all('SELECT * FROM rules');
		const sources = await db.all('SELECT * FROM devices WHERE devType LIKE \"passive\"');
		console.log(rowsDev)
		const targets = await db.all('SELECT * FROM devices WHERE devType LIKE \"active\"');
		res.render('rules_new', { rowsDev, rowsRules,sources, targets});
	} catch (error) {
	 	res.status(500).json({ msg: 'Błąd bazy danych', error });
	}
});

server.listen({
	port: 3000,
});

async function wirteToActive(row) {
	const tDev = connectedClients.find((worker) => { return worker.obj.mac == row.ruleTargetId; });
	if(tDev === undefined) return;
	try {
		console.log(row.ruleTargetName + " " + row.result.toString() + " "+tDev.obj.mac);
		await db.run('INSERT INTO devReadout(espId ,devName, readOut) VALUES(?,?,?)', [row.ruleTargetId, row.ruleTargetName,row.result]);
		tDev.socket.write(row.ruleTargetName + " " + row.result.toString()+"\0");

	} catch (error) {
		console.error(error.message());
	}
}
async function handlemsg(data, client){
	if (data.toString().indexOf('identifier') !== -1) {
		const json = data.toString().substr(11);
		const obj = JSON.parse(json);
		client.obj = obj;
		console.table(connectedClients);
		client.obj.devices.forEach(async (dev) => {
			try {
				await db.run('INSERT INTO devices VALUES(?,?,?,?)', [client.obj.mac, dev.name, dev.type, dev.units]);
				console.log('Succesfuly wrote to devices\n');
			} catch (error) {
				console.error(error.message());
			}
		});
	return;
	}
	client.obj.devices.forEach(async (dev) => {
		if (data.toString().indexOf(dev.name) == ! -1 && dev.type === 'passive') {
			const val = data.toString().substr(data.toString().indexOf(dev.name) + dev.name.length + 1)
			try {
				await db.run('INSERT INTO devReadout(espId ,devName, readOut) VALUES(?,?,?)', [client.obj.mac, dev.name, val]);
				rows = await db.all('SELECT * FROM rules WHERE ruleSourceId = ? AND ruleSourceName = ?',[client.obj.mac, dev.name]);
				rows.forEach((row) => {
					switch (row.ruleFunction) {
						case "==":
							if (val == row.val) {
								wirteToActive(row);
							}
							break;
						case ">=":
							if (val >= row.val) {
								wirteToActive(row);
							}
							break;
						case "<=":
							if (val <= row.val) {
								wirteToActive(row);
							}
							break;
						case ">":
							if (val > row.val) {
								wirteToActive(row);
							}
							break;
						case "<":
							if (val < row.val) {
								wirteToActive(row);
							}
							break;
						case "!=":
							if (val != row.val) {
								wirteToActive(row);
							}
							break;
						default:

					}
				});
			} catch (error) {
				console.error(error.message);
			}
			return;
		}
	});
}

server.on('connection', async (socket) => {
	console.log('Client conected');
	const client = new Client(socket);
	connectedClients.push(client);
	client.socket.on('data', data => {
		const chunks = String(data).split('\0');
		chunks.forEach((chunk)=> handlemsg(chunk,client));
	});
	client.socket.setTimeout(2000,() => {
		console.log("Disconnecting client "+ client.obj.mac);
		connectedClients.splice(connectedClients.indexOf(client),1);
		client.socket.destroy();
	});
	try {
		const rows = await db.all('SELECT * FROM devices');
		rows.forEach((row) => {
			console.log(row);
		})
	} catch (error) {
		console.error(error.message);
	}
});

app.listen(5000);
