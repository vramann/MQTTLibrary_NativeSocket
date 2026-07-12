# Investment Portfolio Tracker

A full-stack tracker for market investments — **equity shares, mutual funds
(lumpsum & SIP), government bonds and sovereign gold bonds (SGB)** — with a
dedicated **Dividends & Income** page and **Excel export for every section**.

| Area | Stack |
|---|---|
| Frontend | React 19 + TypeScript + Vite, Recharts, React Router |
| Backend | Node.js + Express 5, built-in `node:sqlite` (no native deps), ExcelJS |
| Database | SQLite (single file, WAL mode) |

## Features

- **Dashboard** — portfolio value vs. amount invested over time, allocation
  donut by asset class, unrealized P&L per class, all-holdings table.
- **Separate pages per asset class** — Equity, Mutual Funds, Government Bonds
  and Gold Bonds (SGB) each get their own page with class-specific columns
  (coupon %, maturity for bonds), KPI tiles and per-instrument price/NAV history.
- **SIP management** — start/pause SIPs, record monthly installments at that
  day's NAV; units, installment count and invested amount tracked per plan.
- **Dividends & Income page** — equity dividends, MF payouts (IDCW), bond
  coupons and SGB interest tracked separately; monthly stacked chart, fiscal-year
  totals, top income sources, and manual income entry.
- **Transactions** — every buy/sell (including SIP installments), filterable by
  asset class, with delete + add forms.
- **Excel export everywhere** — every page has an export button; the dashboard
  exports a full workbook (Summary + one sheet per asset class + SIPs +
  Dividends + Transactions) with styled headers, number formats and totals.
- **Holdings math** — weighted-average cost basis, unrealized/realized P&L,
  income received per instrument.
- Light & dark mode, colorblind-validated chart palette.

## Quick start

```bash
# 1. Backend (port 4000)
cd backend
npm install
npm run seed        # loads realistic demo data (optional)
npm start

# 2. Frontend dev server (port 5173, proxies /api to :4000)
cd ../frontend
npm install
npm run dev
```

**Production:** `npm run build` in `frontend/` — the backend automatically
serves `frontend/dist` at http://localhost:4000, so one process runs the whole app.

## API overview

| Endpoint | Purpose |
|---|---|
| `GET /api/summary` | Totals + per-asset-class rollup |
| `GET /api/history?months=36` | Monthly portfolio value vs. invested series |
| `GET /api/holdings?asset_class=EQUITY` | Computed holdings (all or per class) |
| `GET/POST /api/instruments` | Instrument catalogue |
| `GET/POST/DELETE /api/transactions` | Buys & sells |
| `GET/POST/PATCH /api/sips`, `POST /api/sips/:id/execute` | SIP plans & installments |
| `GET/POST/DELETE /api/dividends`, `GET /api/dividends/summary` | Income records & rollups |
| `GET/POST /api/prices` | Price/NAV history per instrument |
| `GET /api/export/{portfolio,holdings,transactions,dividends,sips}` | Excel downloads (`?asset_class=` supported) |

See [PLAN.md](PLAN.md) for the architecture, data model and roadmap.

## Screens

Seven pages: Dashboard · Equity · Mutual Funds & SIP · Government Bonds ·
Gold Bonds (SGB) · Dividends & Income · Transactions — all responsive, all
with light/dark themes, all exportable to Excel.
