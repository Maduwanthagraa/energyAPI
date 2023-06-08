const express = require('express');
const sql = require('mssql');

// Create an Express application
const app = express();
const port = 3000; // or any other port you prefer

// Set up middleware to parse request body
app.use(express.json());
app.use(express.urlencoded({ extended: false }));

// Configure the connection pool
const config = {
  user: 'energymeter',
  password: 'Dd@12345',
  server: 'energymeterserver.database.windows.net',
  database: 'energymeterdatabase',
  options: {
    encrypt: true,
    enableArithAbort: true,
    trustServerCertificate: true,
    connectionTimeout: 30000,
    requestTimeout: 60000,
    pool: {
      max: 10, // Maximum number of connections in the pool
      min: 0, // Minimum number of connections in the pool
      idleTimeoutMillis: 30000 // How long a connection can be idle before being released
    }
  }
};

const pool = new sql.ConnectionPool(config);

pool.connect().then(() => {
  console.log('Connected to the database');

  // Define your API routes and request handling logic
  app.get('/', (req, res) => {
    res.send('Welcome to your API');
  });

  app.post('/data', (req, res) => {
    const { id, days, energyvalue } = req.body; // Assuming you have three fields in your data

    const ps = new sql.PreparedStatement(pool);
    ps.input('id', sql.VarChar(50));
    ps.input('days', sql.Date);
    ps.input('energyvalue', sql.Float);

    const insertQuery = 'INSERT INTO energy_data (id, days, energyvalue) VALUES (@id, @days, @energyvalue)';

    ps.prepare(insertQuery, (err) => {
      if (err) {
        console.error('Error preparing the statement: ', err);
        res.sendStatus(500);
        return;
      }

      ps.execute({ id, days, energyvalue }, (err, result) => {
        if (err) {
          console.error('Error inserting data: ', err);
          res.sendStatus(500);
          return;
        }

        ps.unprepare((err) => {
          if (err) {
            console.error('Error unpreparing the statement: ', err);
          }
        });

        res.sendStatus(200);
      });
    });
  });

  // Start the server
  app.listen(port, () => {
    console.log(`Server listening on port ${port}`);
  });
}).catch((err) => {
  console.error('Error connecting to the database: ', err);
});
