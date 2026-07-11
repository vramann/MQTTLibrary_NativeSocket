/**
 * Seeds the database with realistic demo data (Indian market, INR):
 * equities, mutual funds (lumpsum + SIP), government bonds, sovereign gold
 * bonds, monthly price history, and dividend/coupon/interest records.
 *
 * Deterministic (seeded RNG) so repeated runs produce identical data.
 * Run: npm run seed
 */
const db = require('./db');

const TODAY = new Date().toISOString().slice(0, 10);
const START_MONTH = '2023-07';

function mulberry32(seed) {
  let a = seed >>> 0;
  return () => {
    a |= 0;
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function monthEnds(fromMonth, toDate) {
  const [fy, fm] = fromMonth.split('-').map(Number);
  const out = [];
  let y = fy;
  let m = fm;
  for (;;) {
    const end = new Date(Date.UTC(y, m, 0)).toISOString().slice(0, 10);
    if (end > toDate) break;
    out.push(end);
    m += 1;
    if (m > 12) {
      m = 1;
      y += 1;
    }
  }
  return out;
}

/** Geometric walk from start to ~target across the month grid. */
function priceSeries(rand, start, target, dates, vol) {
  const n = dates.length;
  const drift = Math.pow(target / start, 1 / Math.max(n - 1, 1)) - 1;
  const out = [];
  let p = start;
  for (let i = 0; i < n; i++) {
    out.push({ date: dates[i], price: round(p, 2) });
    p *= 1 + drift + vol * (rand() * 2 - 1);
  }
  return out;
}

function priceAsOf(series, date) {
  let px = series[0].price;
  for (const row of series) {
    if (row.date > date) break;
    px = row.price;
  }
  return px;
}

function round(x, n = 2) {
  const f = 10 ** n;
  return Math.round(x * f) / f;
}

// ---------------------------------------------------------------------------

db.exec('DELETE FROM dividends; DELETE FROM transactions; DELETE FROM sips; DELETE FROM prices; DELETE FROM instruments;');
db.exec("DELETE FROM sqlite_sequence WHERE name IN ('instruments','transactions','sips','dividends');");

const insInstrument = db.prepare(
  `INSERT INTO instruments (symbol, name, asset_class, coupon_rate, face_value, maturity_date, notes)
   VALUES (?, ?, ?, ?, ?, ?, ?)`
);
const insPrice = db.prepare('INSERT INTO prices (instrument_id, date, price) VALUES (?, ?, ?)');
const insTx = db.prepare(
  `INSERT INTO transactions (instrument_id, date, type, quantity, price, charges, source, sip_id, notes)
   VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`
);
const insSip = db.prepare(
  'INSERT INTO sips (instrument_id, amount, day_of_month, start_date, end_date, active) VALUES (?, ?, ?, ?, ?, ?)'
);
const insDiv = db.prepare(
  'INSERT INTO dividends (instrument_id, date, type, amount, notes) VALUES (?, ?, ?, ?, ?)'
);

const grid = monthEnds(START_MONTH, TODAY);
const rand = mulberry32(20260711);

const catalog = [
  // symbol, name, class, start px, target px, vol, coupon, face, maturity
  ['RELIANCE', 'Reliance Industries Ltd', 'EQUITY', 2410, 3120, 0.05],
  ['TCS', 'Tata Consultancy Services Ltd', 'EQUITY', 3340, 4180, 0.045],
  ['INFY', 'Infosys Ltd', 'EQUITY', 1395, 1760, 0.055],
  ['HDFCBANK', 'HDFC Bank Ltd', 'EQUITY', 1610, 1935, 0.04],
  ['ITC', 'ITC Ltd', 'EQUITY', 445, 512, 0.04],
  ['UTINIFTY', 'UTI Nifty 50 Index Fund Direct-Growth', 'MUTUAL_FUND', 131.2, 186.5, 0.03],
  ['PPFCF', 'Parag Parikh Flexi Cap Fund Direct-Growth', 'MUTUAL_FUND', 55.6, 93.4, 0.032],
  ['HDFCBAF', 'HDFC Balanced Advantage Fund Direct-IDCW', 'MUTUAL_FUND', 35.1, 38.9, 0.02],
  ['GS2033', '7.18% Government Security 2033', 'GOV_BOND', 99.4, 101.8, 0.006, 7.18, 100, '2033-08-14'],
  ['GS2030', '7.10% Government Security 2030', 'GOV_BOND', 99.8, 101.2, 0.005, 7.1, 100, '2030-04-18'],
  ['SGB2023II', 'Sovereign Gold Bond 2023-24 Series II', 'GOLD_BOND', 5920, 9650, 0.035, 2.5, 5923, '2031-09-20'],
  ['SGB2024I', 'Sovereign Gold Bond 2024-25 Series I', 'GOLD_BOND', 7420, 9650, 0.035, 2.5, 7425, '2032-06-21'],
];

const inst = {}; // symbol -> { id, series, ...meta }
for (const [symbol, name, ac, p0, p1, vol, coupon = null, face = null, maturity = null] of catalog) {
  const id = Number(insInstrument.run(symbol, name, ac, coupon, face, maturity, null).lastInsertRowid);
  const series = priceSeries(rand, p0, p1, grid, vol);
  // ensure a fresh "today" price so current values aren't a month stale
  if (series[series.length - 1].date !== TODAY) {
    series.push({ date: TODAY, price: round(series[series.length - 1].price * (1 + 0.01 * (rand() * 2 - 1)), 2) });
  }
  for (const row of series) insPrice.run(id, row.date, row.price);
  inst[symbol] = { id, symbol, series, coupon, face };
}

// --- Equity lumpsum transactions -------------------------------------------
const buy = (sym, date, qty, extraPct = 0, notes = null) => {
  const px = round(priceAsOf(inst[sym].series, date) * (1 + extraPct), 2);
  insTx.run(inst[sym].id, date, 'BUY', qty, px, round(qty * px * 0.001), 'LUMPSUM', null, notes);
};
const sell = (sym, date, qty, extraPct = 0, notes = null) => {
  const px = round(priceAsOf(inst[sym].series, date) * (1 + extraPct), 2);
  insTx.run(inst[sym].id, date, 'SELL', qty, px, round(qty * px * 0.001), 'LUMPSUM', null, notes);
};

buy('RELIANCE', '2023-08-10', 40, -0.01);
buy('RELIANCE', '2024-03-18', 25, 0.005);
buy('TCS', '2023-09-05', 30, -0.008);
sell('TCS', '2025-02-12', 10, 0.01, 'Partial profit booking');
buy('INFY', '2023-10-16', 100, -0.012);
buy('INFY', '2025-04-07', 50, 0.004);
buy('HDFCBANK', '2023-08-22', 60, 0.002);
buy('HDFCBANK', '2024-11-14', 40, -0.006);
buy('ITC', '2023-07-31', 400, -0.005);
buy('ITC', '2024-06-20', 200, 0.003);
sell('ITC', '2026-01-15', 100, 0.008, 'Rebalancing');

// --- Mutual funds: lumpsum + SIPs -------------------------------------------
buy('HDFCBAF', '2023-09-12', round(200000 / priceAsOf(inst.HDFCBAF.series, '2023-09-12'), 3), 0, 'Lumpsum investment');

const sips = [
  { sym: 'UTINIFTY', amount: 10000, day: 5, start: '2024-01-05' },
  { sym: 'PPFCF', amount: 5000, day: 10, start: '2023-08-10' },
];
for (const s of sips) {
  const sipId = Number(insSip.run(inst[s.sym].id, s.amount, s.day, s.start, null, 1).lastInsertRowid);
  let [y, m] = s.start.split('-').map(Number);
  for (;;) {
    const date = `${y}-${String(m).padStart(2, '0')}-${String(s.day).padStart(2, '0')}`;
    if (date > TODAY) break;
    if (date >= s.start) {
      const nav = round(priceAsOf(inst[s.sym].series, date) * (1 + 0.01 * (rand() * 2 - 1)), 4);
      insTx.run(inst[s.sym].id, date, 'BUY', round(s.amount / nav, 3), nav, 0, 'SIP', sipId, 'SIP installment');
    }
    m += 1;
    if (m > 12) {
      m = 1;
      y += 1;
    }
  }
}

// --- Government bonds & SGBs -------------------------------------------------
buy('GS2033', '2023-08-14', 300, 0, 'RBI Retail Direct');
buy('GS2030', '2024-02-05', 200, 0, 'RBI Retail Direct');
buy('SGB2023II', '2023-09-20', 10, 0, 'Primary issue, 10 g');
buy('SGB2024I', '2024-06-21', 15, 0, 'Primary issue, 15 g');

// --- Dividends / coupons / interest -----------------------------------------
const qtyAsOf = (sym, date) => {
  const rows = db
    .prepare('SELECT type, quantity FROM transactions WHERE instrument_id = ? AND date <= ?')
    .all(inst[sym].id, date);
  return rows.reduce((q, r) => q + (r.type === 'BUY' ? r.quantity : -r.quantity), 0);
};

// Equity dividends: [symbol, per-share amounts by (approx) half-year]
const equityDividends = {
  RELIANCE: [['2023-08-28', 9], ['2024-08-19', 10], ['2025-08-25', 10.5]],
  TCS: [
    ['2023-10-18', 9], ['2024-01-19', 27], ['2024-05-16', 28], ['2024-10-25', 10],
    ['2025-01-20', 30], ['2025-05-30', 30], ['2025-10-27', 11], ['2026-01-19', 32],
  ],
  INFY: [['2023-10-25', 18], ['2024-05-31', 28], ['2024-10-29', 21], ['2025-05-30', 29], ['2025-10-28', 23]],
  HDFCBANK: [['2024-05-10', 19.5], ['2025-05-09', 22]],
  ITC: [
    ['2023-08-08', 6.75], ['2024-02-21', 6.25], ['2024-07-05', 7.5], ['2025-02-20', 6.5],
    ['2025-07-04', 7.85], ['2026-02-19', 6.8],
  ],
};
for (const [sym, rows] of Object.entries(equityDividends)) {
  for (const [date, perShare] of rows) {
    if (date > TODAY) continue;
    const q = qtyAsOf(sym, date);
    if (q > 0) insDiv.run(inst[sym].id, date, 'DIVIDEND', round(q * perShare), `₹${perShare}/share on ${q} shares`);
  }
}

// MF IDCW payouts: quarterly ₹0.30/unit on HDFC BAF
for (const date of ['2023-12-26', '2024-03-26', '2024-06-25', '2024-09-25', '2024-12-24', '2025-03-25', '2025-06-25', '2025-09-25', '2025-12-23', '2026-03-25', '2026-06-25']) {
  if (date > TODAY) continue;
  const q = qtyAsOf('HDFCBAF', date);
  if (q > 0) insDiv.run(inst.HDFCBAF.id, date, 'MF_PAYOUT', round(q * 0.3), 'IDCW payout ₹0.30/unit');
}

// Semi-annual coupons on G-Secs and SGB interest (2.5% p.a. on issue price)
function semiAnnual(sym, firstDate, type) {
  const { coupon, face } = inst[sym];
  let d = new Date(firstDate + 'T00:00:00Z');
  for (;;) {
    const date = d.toISOString().slice(0, 10);
    if (date > TODAY) break;
    const q = qtyAsOf(sym, date);
    if (q > 0) {
      const amt = round((q * face * coupon) / 100 / 2);
      insDiv.run(inst[sym].id, date, type, amt, `${coupon}% p.a. half-yearly on ${q} units`);
    }
    d = new Date(Date.UTC(d.getUTCFullYear(), d.getUTCMonth() + 6, d.getUTCDate()));
  }
}
semiAnnual('GS2033', '2024-02-14', 'COUPON');
semiAnnual('GS2030', '2024-10-18', 'COUPON');
semiAnnual('SGB2023II', '2024-03-20', 'SGB_INTEREST');
semiAnnual('SGB2024I', '2024-12-21', 'SGB_INTEREST');

const counts = {
  instruments: db.prepare('SELECT COUNT(*) c FROM instruments').get().c,
  prices: db.prepare('SELECT COUNT(*) c FROM prices').get().c,
  transactions: db.prepare('SELECT COUNT(*) c FROM transactions').get().c,
  sips: db.prepare('SELECT COUNT(*) c FROM sips').get().c,
  dividends: db.prepare('SELECT COUNT(*) c FROM dividends').get().c,
};
console.log('Seeded:', counts);
