const db = require('../db');

const ASSET_CLASSES = ['EQUITY', 'MUTUAL_FUND', 'GOV_BOND', 'GOLD_BOND', 'OTHER'];

function latestPrice(instrumentId) {
  return db
    .prepare('SELECT price, date FROM prices WHERE instrument_id = ? ORDER BY date DESC LIMIT 1')
    .get(instrumentId);
}

/**
 * Weighted-average-cost holdings per instrument.
 * BUY:  cost basis += qty*price + charges
 * SELL: realized P&L += proceeds - avg_cost*qty; cost basis -= avg_cost*qty
 */
function computeHoldings(assetClass) {
  const instruments = assetClass
    ? db.prepare('SELECT * FROM instruments WHERE asset_class = ? ORDER BY symbol').all(assetClass)
    : db.prepare('SELECT * FROM instruments ORDER BY asset_class, symbol').all();

  const txStmt = db.prepare(
    'SELECT * FROM transactions WHERE instrument_id = ? ORDER BY date, id'
  );
  const divStmt = db.prepare(
    'SELECT COALESCE(SUM(amount), 0) AS total FROM dividends WHERE instrument_id = ?'
  );

  const holdings = [];
  for (const inst of instruments) {
    let qty = 0;
    let costBasis = 0;
    let realizedPnl = 0;
    let investedGross = 0; // total money ever put in (for reference)

    for (const tx of txStmt.all(inst.id)) {
      if (tx.type === 'BUY') {
        costBasis += tx.quantity * tx.price + tx.charges;
        investedGross += tx.quantity * tx.price + tx.charges;
        qty += tx.quantity;
      } else {
        const avgCost = qty > 0 ? costBasis / qty : 0;
        const sellQty = Math.min(tx.quantity, qty);
        realizedPnl += tx.quantity * tx.price - tx.charges - avgCost * sellQty;
        costBasis -= avgCost * sellQty;
        qty -= sellQty;
      }
    }

    const lp = latestPrice(inst.id);
    const currentPrice = lp ? lp.price : null;
    const currentValue = currentPrice != null ? qty * currentPrice : null;
    const dividends = divStmt.get(inst.id).total;

    holdings.push({
      instrument_id: inst.id,
      symbol: inst.symbol,
      name: inst.name,
      asset_class: inst.asset_class,
      coupon_rate: inst.coupon_rate,
      face_value: inst.face_value,
      maturity_date: inst.maturity_date,
      quantity: round(qty, 4),
      avg_cost: qty > 0 ? round(costBasis / qty, 4) : 0,
      invested: round(costBasis, 2),
      invested_gross: round(investedGross, 2),
      current_price: currentPrice,
      price_date: lp ? lp.date : null,
      current_value: currentValue != null ? round(currentValue, 2) : null,
      unrealized_pnl: currentValue != null ? round(currentValue - costBasis, 2) : null,
      unrealized_pnl_pct:
        currentValue != null && costBasis > 0
          ? round(((currentValue - costBasis) / costBasis) * 100, 2)
          : null,
      realized_pnl: round(realizedPnl, 2),
      dividends_received: round(dividends, 2),
    });
  }
  return holdings;
}

function summary() {
  const holdings = computeHoldings();
  const byClass = {};
  for (const ac of ASSET_CLASSES) {
    byClass[ac] = {
      asset_class: ac,
      invested: 0,
      current_value: 0,
      unrealized_pnl: 0,
      realized_pnl: 0,
      dividends_received: 0,
      holdings_count: 0,
    };
  }
  for (const h of holdings) {
    const s = byClass[h.asset_class];
    s.invested += h.invested;
    s.current_value += h.current_value || 0;
    s.unrealized_pnl += h.unrealized_pnl || 0;
    s.realized_pnl += h.realized_pnl;
    s.dividends_received += h.dividends_received;
    if (h.quantity > 0) s.holdings_count += 1;
  }
  const classes = Object.values(byClass)
    .map((s) => ({
      ...s,
      invested: round(s.invested, 2),
      current_value: round(s.current_value, 2),
      unrealized_pnl: round(s.unrealized_pnl, 2),
      realized_pnl: round(s.realized_pnl, 2),
      dividends_received: round(s.dividends_received, 2),
      pnl_pct: s.invested > 0 ? round((s.unrealized_pnl / s.invested) * 100, 2) : 0,
    }))
    .filter((s) => s.invested > 0 || s.current_value > 0 || s.realized_pnl !== 0);

  const total = classes.reduce(
    (acc, s) => {
      acc.invested += s.invested;
      acc.current_value += s.current_value;
      acc.unrealized_pnl += s.unrealized_pnl;
      acc.realized_pnl += s.realized_pnl;
      acc.dividends_received += s.dividends_received;
      return acc;
    },
    { invested: 0, current_value: 0, unrealized_pnl: 0, realized_pnl: 0, dividends_received: 0 }
  );
  total.pnl_pct = total.invested > 0 ? round((total.unrealized_pnl / total.invested) * 100, 2) : 0;
  for (const k of ['invested', 'current_value', 'unrealized_pnl', 'realized_pnl', 'dividends_received'])
    total[k] = round(total[k], 2);

  return { total, by_asset_class: classes };
}

/**
 * Monthly portfolio value/invested series (month-ends + today),
 * valuing each instrument at the latest price on or before each date.
 */
function history(months = 36) {
  const dates = [];
  const now = new Date();
  for (let i = months; i >= 1; i--) {
    const d = new Date(Date.UTC(now.getUTCFullYear(), now.getUTCMonth() - i + 1, 0));
    dates.push(d.toISOString().slice(0, 10));
  }
  dates.push(now.toISOString().slice(0, 10));

  const instruments = db.prepare('SELECT id, asset_class FROM instruments').all();
  const txAll = db
    .prepare('SELECT instrument_id, date, type, quantity, price, charges FROM transactions ORDER BY date, id')
    .all();
  const priceRows = {};
  for (const inst of instruments) {
    priceRows[inst.id] = db
      .prepare('SELECT date, price FROM prices WHERE instrument_id = ? ORDER BY date')
      .all(inst.id);
  }

  const state = {}; // instrument_id -> { qty, cost }
  for (const inst of instruments) state[inst.id] = { qty: 0, cost: 0 };
  const pricePtr = {};
  for (const inst of instruments) pricePtr[inst.id] = -1;

  let txPtr = 0;
  const series = [];
  for (const date of dates) {
    while (txPtr < txAll.length && txAll[txPtr].date <= date) {
      const tx = txAll[txPtr++];
      const s = state[tx.instrument_id];
      if (!s) continue;
      if (tx.type === 'BUY') {
        s.cost += tx.quantity * tx.price + tx.charges;
        s.qty += tx.quantity;
      } else {
        const avg = s.qty > 0 ? s.cost / s.qty : 0;
        const q = Math.min(tx.quantity, s.qty);
        s.cost -= avg * q;
        s.qty -= q;
      }
    }
    let value = 0;
    let invested = 0;
    for (const inst of instruments) {
      const s = state[inst.id];
      if (s.qty <= 0) continue;
      const rows = priceRows[inst.id];
      let p = pricePtr[inst.id];
      while (p + 1 < rows.length && rows[p + 1].date <= date) p++;
      pricePtr[inst.id] = p;
      const px = p >= 0 ? rows[p].price : null;
      if (px != null) value += s.qty * px;
      invested += s.cost;
    }
    series.push({ date, value: round(value, 2), invested: round(invested, 2) });
  }
  return series;
}

function dividendSummary() {
  const byMonth = db
    .prepare(
      `SELECT substr(date, 1, 7) AS month, type, SUM(amount) AS amount
       FROM dividends GROUP BY month, type ORDER BY month`
    )
    .all();
  const byInstrument = db
    .prepare(
      `SELECT i.symbol, i.name, i.asset_class, d.type, SUM(d.amount) AS amount, COUNT(*) AS payouts
       FROM dividends d JOIN instruments i ON i.id = d.instrument_id
       GROUP BY d.instrument_id, d.type ORDER BY amount DESC`
    )
    .all();
  const byFY = db
    .prepare(
      `SELECT CASE WHEN CAST(substr(date, 6, 2) AS INTEGER) >= 4
              THEN 'FY' || substr(date, 3, 2) || '-' || printf('%02d', (CAST(substr(date, 1, 4) AS INTEGER) + 1) % 100)
              ELSE 'FY' || printf('%02d', (CAST(substr(date, 1, 4) AS INTEGER) - 1) % 100) || '-' || substr(date, 3, 2)
              END AS fiscal_year,
              SUM(amount) AS amount
       FROM dividends GROUP BY fiscal_year ORDER BY fiscal_year`
    )
    .all();
  const total = db.prepare('SELECT COALESCE(SUM(amount), 0) AS total FROM dividends').get().total;
  return { total: round(total, 2), by_month: byMonth, by_instrument: byInstrument, by_fiscal_year: byFY };
}

function round(x, n) {
  const f = 10 ** n;
  return Math.round(x * f) / f;
}

module.exports = { computeHoldings, summary, history, dividendSummary, ASSET_CLASSES };
