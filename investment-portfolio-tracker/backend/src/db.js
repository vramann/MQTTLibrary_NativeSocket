const { DatabaseSync } = require('node:sqlite');
const path = require('node:path');
const fs = require('node:fs');

const DATA_DIR = process.env.DATA_DIR || path.join(__dirname, '..', 'data');
fs.mkdirSync(DATA_DIR, { recursive: true });

const db = new DatabaseSync(path.join(DATA_DIR, 'portfolio.db'));

db.exec(`
PRAGMA journal_mode = WAL;
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS instruments (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  symbol        TEXT NOT NULL UNIQUE,
  name          TEXT NOT NULL,
  asset_class   TEXT NOT NULL CHECK (asset_class IN ('EQUITY','MUTUAL_FUND','GOV_BOND','GOLD_BOND','OTHER')),
  currency      TEXT NOT NULL DEFAULT 'INR',
  coupon_rate   REAL,
  face_value    REAL,
  maturity_date TEXT,
  notes         TEXT
);

CREATE TABLE IF NOT EXISTS prices (
  instrument_id INTEGER NOT NULL REFERENCES instruments(id) ON DELETE CASCADE,
  date          TEXT NOT NULL,
  price         REAL NOT NULL,
  PRIMARY KEY (instrument_id, date)
);

CREATE TABLE IF NOT EXISTS sips (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  instrument_id INTEGER NOT NULL REFERENCES instruments(id) ON DELETE CASCADE,
  amount        REAL NOT NULL,
  day_of_month  INTEGER NOT NULL DEFAULT 1,
  start_date    TEXT NOT NULL,
  end_date      TEXT,
  active        INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS transactions (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  instrument_id INTEGER NOT NULL REFERENCES instruments(id) ON DELETE CASCADE,
  date          TEXT NOT NULL,
  type          TEXT NOT NULL CHECK (type IN ('BUY','SELL')),
  quantity      REAL NOT NULL CHECK (quantity > 0),
  price         REAL NOT NULL CHECK (price >= 0),
  charges       REAL NOT NULL DEFAULT 0,
  source        TEXT NOT NULL DEFAULT 'LUMPSUM' CHECK (source IN ('LUMPSUM','SIP')),
  sip_id        INTEGER REFERENCES sips(id) ON DELETE SET NULL,
  notes         TEXT
);

CREATE TABLE IF NOT EXISTS dividends (
  id            INTEGER PRIMARY KEY AUTOINCREMENT,
  instrument_id INTEGER NOT NULL REFERENCES instruments(id) ON DELETE CASCADE,
  date          TEXT NOT NULL,
  type          TEXT NOT NULL CHECK (type IN ('DIVIDEND','COUPON','SGB_INTEREST','MF_PAYOUT','OTHER')),
  amount        REAL NOT NULL,
  notes         TEXT
);

CREATE INDEX IF NOT EXISTS idx_tx_instrument ON transactions(instrument_id, date);
CREATE INDEX IF NOT EXISTS idx_div_date ON dividends(date);
CREATE INDEX IF NOT EXISTS idx_prices_date ON prices(date);
`);

module.exports = db;
