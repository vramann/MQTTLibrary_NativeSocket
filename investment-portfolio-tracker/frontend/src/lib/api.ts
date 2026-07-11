export type AssetClass = 'EQUITY' | 'MUTUAL_FUND' | 'GOV_BOND' | 'GOLD_BOND' | 'OTHER'

export interface Holding {
  instrument_id: number
  symbol: string
  name: string
  asset_class: AssetClass
  coupon_rate: number | null
  face_value: number | null
  maturity_date: string | null
  quantity: number
  avg_cost: number
  invested: number
  current_price: number | null
  price_date: string | null
  current_value: number | null
  unrealized_pnl: number | null
  unrealized_pnl_pct: number | null
  realized_pnl: number
  dividends_received: number
}

export interface ClassSummary {
  asset_class: AssetClass
  invested: number
  current_value: number
  unrealized_pnl: number
  realized_pnl: number
  dividends_received: number
  holdings_count: number
  pnl_pct: number
}

export interface Summary {
  total: Omit<ClassSummary, 'asset_class' | 'holdings_count'>
  by_asset_class: ClassSummary[]
}

export interface HistoryPoint {
  date: string
  value: number
  invested: number
}

export interface Transaction {
  id: number
  instrument_id: number
  date: string
  type: 'BUY' | 'SELL'
  quantity: number
  price: number
  charges: number
  source: 'LUMPSUM' | 'SIP'
  notes: string | null
  symbol: string
  name: string
  asset_class: AssetClass
}

export interface Sip {
  id: number
  instrument_id: number
  amount: number
  day_of_month: number
  start_date: string
  end_date: string | null
  active: number
  symbol: string
  name: string
  installments: number
  invested: number
  units: number
}

export interface Dividend {
  id: number
  instrument_id: number
  date: string
  type: 'DIVIDEND' | 'COUPON' | 'SGB_INTEREST' | 'MF_PAYOUT' | 'OTHER'
  amount: number
  notes: string | null
  symbol: string
  name: string
  asset_class: AssetClass
}

export interface DividendSummary {
  total: number
  by_month: { month: string; type: Dividend['type']; amount: number }[]
  by_instrument: {
    symbol: string
    name: string
    asset_class: AssetClass
    type: Dividend['type']
    amount: number
    payouts: number
  }[]
  by_fiscal_year: { fiscal_year: string; amount: number }[]
}

export interface Instrument {
  id: number
  symbol: string
  name: string
  asset_class: AssetClass
  coupon_rate: number | null
  face_value: number | null
  maturity_date: string | null
}

export async function api<T>(path: string, init?: RequestInit): Promise<T> {
  const res = await fetch(`/api${path}`, {
    headers: { 'Content-Type': 'application/json' },
    ...init,
  })
  if (!res.ok) {
    const body = await res.json().catch(() => ({}))
    throw new Error(body.error || `${res.status} ${res.statusText}`)
  }
  return res.json()
}

export const CLASS_META: Record<AssetClass, { label: string; cssVar: string }> = {
  EQUITY: { label: 'Equity', cssVar: 'var(--c-equity)' },
  MUTUAL_FUND: { label: 'Mutual Funds', cssVar: 'var(--c-mf)' },
  GOV_BOND: { label: 'Govt Bonds', cssVar: 'var(--c-govbond)' },
  GOLD_BOND: { label: 'Gold Bonds (SGB)', cssVar: 'var(--c-goldbond)' },
  OTHER: { label: 'Other', cssVar: 'var(--c-other)' },
}

/* Income types inherit the hue of the asset class that produces them,
   so identity stays consistent across pages. */
export const INCOME_META: Record<Dividend['type'], { label: string; cssVar: string }> = {
  DIVIDEND: { label: 'Equity dividends', cssVar: 'var(--c-equity)' },
  MF_PAYOUT: { label: 'MF payouts (IDCW)', cssVar: 'var(--c-mf)' },
  COUPON: { label: 'Bond coupons', cssVar: 'var(--c-govbond)' },
  SGB_INTEREST: { label: 'SGB interest', cssVar: 'var(--c-goldbond)' },
  OTHER: { label: 'Other income', cssVar: 'var(--c-other)' },
}

const inr = new Intl.NumberFormat('en-IN', {
  style: 'currency',
  currency: 'INR',
  maximumFractionDigits: 0,
})
const inr2 = new Intl.NumberFormat('en-IN', {
  style: 'currency',
  currency: 'INR',
  minimumFractionDigits: 2,
  maximumFractionDigits: 2,
})

export const fmtINR = (v: number | null | undefined) => (v == null ? '—' : inr.format(v))
export const fmtINR2 = (v: number | null | undefined) => (v == null ? '—' : inr2.format(v))

/** Compact Indian notation: ₹12.4L, ₹1.02Cr */
export function fmtCompact(v: number | null | undefined): string {
  if (v == null) return '—'
  const sign = v < 0 ? '-' : ''
  const a = Math.abs(v)
  if (a >= 1e7) return `${sign}₹${(a / 1e7).toFixed(2)}Cr`
  if (a >= 1e5) return `${sign}₹${(a / 1e5).toFixed(1)}L`
  if (a >= 1e3) return `${sign}₹${(a / 1e3).toFixed(1)}K`
  return `${sign}₹${a.toFixed(0)}`
}

export const fmtQty = (v: number) =>
  v.toLocaleString('en-IN', { maximumFractionDigits: 3 })

export const fmtPct = (v: number | null | undefined) =>
  v == null ? '—' : `${v >= 0 ? '+' : ''}${v.toFixed(2)}%`

export const fmtMonth = (ym: string) => {
  const [y, m] = ym.split('-')
  return new Date(Number(y), Number(m) - 1, 1).toLocaleString('en', { month: 'short', year: '2-digit' })
}
