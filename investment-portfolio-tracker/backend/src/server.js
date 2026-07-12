const express = require('express');
const cors = require('cors');
const path = require('node:path');
const fs = require('node:fs');
const db = require('./db');
const { computeHoldings, summary, history, dividendSummary, ASSET_CLASSES } = require('./services/portfolio');
const { buildWorkbook } = require('./services/excel');

const app = express();
app.use(cors());
app.use(express.json());

const ok = (res, data) => res.json(data);
const bad = (res, msg) => res.status(400).json({ error: msg });

// --- Portfolio ---------------------------------------------------------------
app.get('/api/summary', (req, res) => ok(res, summary()));
app.get('/api/history', (req, res) => {
  const months = Math.min(Math.max(parseInt(req.query.months, 10) || 36, 1), 120);
  ok(res, history(months));
});
app.get('/api/holdings', (req, res) => {
  const ac = req.query.asset_class;
  if (ac && !ASSET_CLASSES.includes(ac)) return bad(res, 'invalid asset_class');
  ok(res, computeHoldings(ac || undefined));
});

// --- Instruments ---------------------------------------------------------------
app.get('/api/instruments', (req, res) => {
  const ac = req.query.asset_class;
  const rows = ac
    ? db.prepare('SELECT * FROM instruments WHERE asset_class = ? ORDER BY symbol').all(ac)
    : db.prepare('SELECT * FROM instruments ORDER BY asset_class, symbol').all();
  ok(res, rows);
});
app.post('/api/instruments', (req, res) => {
  const { symbol, name, asset_class, coupon_rate, face_value, maturity_date, notes } = req.body || {};
  if (!symbol || !name || !ASSET_CLASSES.includes(asset_class))
    return bad(res, 'symbol, name and valid asset_class are required');
  try {
    const r = db
      .prepare(
        `INSERT INTO instruments (symbol, name, asset_class, coupon_rate, face_value, maturity_date, notes)
         VALUES (?, ?, ?, ?, ?, ?, ?)`
      )
      .run(symbol.toUpperCase(), name, asset_class, coupon_rate ?? null, face_value ?? null, maturity_date ?? null, notes ?? null);
    ok(res, db.prepare('SELECT * FROM instruments WHERE id = ?').get(Number(r.lastInsertRowid)));
  } catch (e) {
    bad(res, e.message.includes('UNIQUE') ? 'symbol already exists' : e.message);
  }
});

// --- Prices ---------------------------------------------------------------
app.get('/api/prices/:instrumentId', (req, res) => {
  ok(
    res,
    db
      .prepare('SELECT date, price FROM prices WHERE instrument_id = ? ORDER BY date')
      .all(Number(req.params.instrumentId))
  );
});
app.post('/api/prices', (req, res) => {
  const { instrument_id, date, price } = req.body || {};
  if (!instrument_id || !date || price == null || price < 0) return bad(res, 'instrument_id, date, price required');
  db.prepare('INSERT OR REPLACE INTO prices (instrument_id, date, price) VALUES (?, ?, ?)').run(
    instrument_id,
    date,
    price
  );
  ok(res, { updated: true });
});

// --- Transactions ---------------------------------------------------------------
app.get('/api/transactions', (req, res) => {
  let sql = `SELECT t.*, i.symbol, i.name, i.asset_class
             FROM transactions t JOIN instruments i ON i.id = t.instrument_id WHERE 1=1`;
  const params = [];
  if (req.query.asset_class) {
    sql += ' AND i.asset_class = ?';
    params.push(req.query.asset_class);
  }
  if (req.query.instrument_id) {
    sql += ' AND t.instrument_id = ?';
    params.push(Number(req.query.instrument_id));
  }
  sql += ' ORDER BY t.date DESC, t.id DESC LIMIT 1000';
  ok(res, db.prepare(sql).all(...params));
});
app.post('/api/transactions', (req, res) => {
  const { instrument_id, date, type, quantity, price, charges, source, notes } = req.body || {};
  if (!instrument_id || !date || !['BUY', 'SELL'].includes(type) || !(quantity > 0) || !(price >= 0))
    return bad(res, 'instrument_id, date, type BUY/SELL, quantity>0, price>=0 required');
  const r = db
    .prepare(
      `INSERT INTO transactions (instrument_id, date, type, quantity, price, charges, source, notes)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?)`
    )
    .run(instrument_id, date, type, quantity, price, charges || 0, source === 'SIP' ? 'SIP' : 'LUMPSUM', notes ?? null);
  // keep a same-day price point so valuations reflect the trade immediately
  db.prepare('INSERT OR REPLACE INTO prices (instrument_id, date, price) VALUES (?, ?, ?)').run(
    instrument_id,
    date,
    price
  );
  ok(res, db.prepare('SELECT * FROM transactions WHERE id = ?').get(Number(r.lastInsertRowid)));
});
app.delete('/api/transactions/:id', (req, res) => {
  db.prepare('DELETE FROM transactions WHERE id = ?').run(Number(req.params.id));
  ok(res, { deleted: true });
});

// --- SIPs ---------------------------------------------------------------
app.get('/api/sips', (req, res) => {
  ok(
    res,
    db
      .prepare(
        `SELECT s.*, i.symbol, i.name,
                COUNT(t.id) AS installments,
                COALESCE(SUM(t.quantity * t.price), 0) AS invested,
                COALESCE(SUM(t.quantity), 0) AS units
         FROM sips s
         JOIN instruments i ON i.id = s.instrument_id
         LEFT JOIN transactions t ON t.sip_id = s.id
         GROUP BY s.id ORDER BY s.active DESC, i.symbol`
      )
      .all()
  );
});
app.post('/api/sips', (req, res) => {
  const { instrument_id, amount, day_of_month, start_date } = req.body || {};
  if (!instrument_id || !(amount > 0) || !day_of_month || !start_date)
    return bad(res, 'instrument_id, amount>0, day_of_month, start_date required');
  const r = db
    .prepare('INSERT INTO sips (instrument_id, amount, day_of_month, start_date, active) VALUES (?, ?, ?, ?, 1)')
    .run(instrument_id, amount, day_of_month, start_date);
  ok(res, db.prepare('SELECT * FROM sips WHERE id = ?').get(Number(r.lastInsertRowid)));
});
app.patch('/api/sips/:id', (req, res) => {
  const sip = db.prepare('SELECT * FROM sips WHERE id = ?').get(Number(req.params.id));
  if (!sip) return bad(res, 'sip not found');
  const active = req.body?.active != null ? (req.body.active ? 1 : 0) : sip.active;
  const amount = req.body?.amount > 0 ? req.body.amount : sip.amount;
  db.prepare('UPDATE sips SET active = ?, amount = ?, end_date = ? WHERE id = ?').run(
    active,
    amount,
    active ? null : new Date().toISOString().slice(0, 10),
    sip.id
  );
  ok(res, db.prepare('SELECT * FROM sips WHERE id = ?').get(sip.id));
});
// Record one SIP installment (e.g. this month's purchase at given NAV)
app.post('/api/sips/:id/execute', (req, res) => {
  const sip = db.prepare('SELECT * FROM sips WHERE id = ?').get(Number(req.params.id));
  if (!sip) return bad(res, 'sip not found');
  const { date, nav } = req.body || {};
  if (!date || !(nav > 0)) return bad(res, 'date and nav>0 required');
  const qty = Math.round((sip.amount / nav) * 1000) / 1000;
  const r = db
    .prepare(
      `INSERT INTO transactions (instrument_id, date, type, quantity, price, charges, source, sip_id, notes)
       VALUES (?, ?, 'BUY', ?, ?, 0, 'SIP', ?, 'SIP installment')`
    )
    .run(sip.instrument_id, date, qty, nav, sip.id);
  db.prepare('INSERT OR REPLACE INTO prices (instrument_id, date, price) VALUES (?, ?, ?)').run(
    sip.instrument_id,
    date,
    nav
  );
  ok(res, db.prepare('SELECT * FROM transactions WHERE id = ?').get(Number(r.lastInsertRowid)));
});

// --- Dividends ---------------------------------------------------------------
app.get('/api/dividends', (req, res) => {
  let sql = `SELECT d.*, i.symbol, i.name, i.asset_class
             FROM dividends d JOIN instruments i ON i.id = d.instrument_id WHERE 1=1`;
  const params = [];
  if (req.query.asset_class) {
    sql += ' AND i.asset_class = ?';
    params.push(req.query.asset_class);
  }
  if (req.query.year) {
    sql += " AND substr(d.date, 1, 4) = ?";
    params.push(String(req.query.year));
  }
  sql += ' ORDER BY d.date DESC LIMIT 1000';
  ok(res, db.prepare(sql).all(...params));
});
app.get('/api/dividends/summary', (req, res) => ok(res, dividendSummary()));
app.post('/api/dividends', (req, res) => {
  const { instrument_id, date, type, amount, notes } = req.body || {};
  const types = ['DIVIDEND', 'COUPON', 'SGB_INTEREST', 'MF_PAYOUT', 'OTHER'];
  if (!instrument_id || !date || !types.includes(type) || !(amount > 0))
    return bad(res, `instrument_id, date, type (${types.join('/')}), amount>0 required`);
  const r = db
    .prepare('INSERT INTO dividends (instrument_id, date, type, amount, notes) VALUES (?, ?, ?, ?, ?)')
    .run(instrument_id, date, type, amount, notes ?? null);
  ok(res, db.prepare('SELECT * FROM dividends WHERE id = ?').get(Number(r.lastInsertRowid)));
});
app.delete('/api/dividends/:id', (req, res) => {
  db.prepare('DELETE FROM dividends WHERE id = ?').run(Number(req.params.id));
  ok(res, { deleted: true });
});

// --- Excel exports ---------------------------------------------------------------
app.get('/api/export/:kind', async (req, res) => {
  const kinds = ['holdings', 'transactions', 'dividends', 'sips', 'portfolio'];
  const { kind } = req.params;
  if (!kinds.includes(kind)) return bad(res, `kind must be one of ${kinds.join(', ')}`);
  try {
    const wb = await buildWorkbook(kind, {
      asset_class: req.query.asset_class,
      instrument_id: req.query.instrument_id ? Number(req.query.instrument_id) : undefined,
    });
    const suffix = req.query.asset_class ? `-${req.query.asset_class.toLowerCase()}` : '';
    const filename = `${kind}${suffix}-${new Date().toISOString().slice(0, 10)}.xlsx`;
    res.setHeader('Content-Type', 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet');
    res.setHeader('Content-Disposition', `attachment; filename="${filename}"`);
    await wb.xlsx.write(res);
    res.end();
  } catch (e) {
    res.status(500).json({ error: e.message });
  }
});

// --- Static frontend (production build) ------------------------------------
const dist = path.join(__dirname, '..', '..', 'frontend', 'dist');
if (fs.existsSync(dist)) {
  app.use(express.static(dist));
  app.use((req, res, next) => {
    if (req.method === 'GET' && !req.path.startsWith('/api/')) {
      return res.sendFile(path.join(dist, 'index.html'));
    }
    next();
  });
}

const PORT = process.env.PORT || 4000;
app.listen(PORT, () => console.log(`Investment tracker API listening on http://localhost:${PORT}`));
