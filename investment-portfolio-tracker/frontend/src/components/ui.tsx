import type { ReactNode } from 'react'
import { NavLink } from 'react-router-dom'
import { CLASS_META, fmtCompact, fmtPct } from '../lib/api'

export function Layout({ children }: { children: ReactNode }) {
  const nav = [
    { to: '/', label: 'Dashboard', dot: 'var(--text-muted)' },
    { to: '/equity', label: 'Equity', dot: CLASS_META.EQUITY.cssVar },
    { to: '/mutual-funds', label: 'Mutual Funds & SIP', dot: CLASS_META.MUTUAL_FUND.cssVar },
    { to: '/gov-bonds', label: 'Government Bonds', dot: CLASS_META.GOV_BOND.cssVar },
    { to: '/gold-bonds', label: 'Gold Bonds (SGB)', dot: CLASS_META.GOLD_BOND.cssVar },
    { to: '/dividends', label: 'Dividends & Income', dot: 'var(--c-other)' },
    { to: '/transactions', label: 'Transactions', dot: 'var(--text-muted)' },
  ]
  return (
    <div className="app">
      <nav className="sidebar">
        <div className="brand">📊 Portfolio Tracker</div>
        {nav.map((n) => (
          <NavLink key={n.to} to={n.to} end={n.to === '/'}>
            <span className="nav-dot" style={{ background: n.dot }} />
            {n.label}
          </NavLink>
        ))}
      </nav>
      <main className="main">{children}</main>
    </div>
  )
}

export function PageHead({
  title,
  sub,
  actions,
}: {
  title: string
  sub?: string
  actions?: ReactNode
}) {
  return (
    <div className="page-head">
      <div>
        <h1>{title}</h1>
        {sub && <div className="sub">{sub}</div>}
      </div>
      <div style={{ display: 'flex', gap: 8 }}>{actions}</div>
    </div>
  )
}

export function StatTile({
  label,
  value,
  delta,
  deltaLabel,
  hero,
}: {
  label: string
  value: string
  delta?: number | null
  deltaLabel?: string
  hero?: boolean
}) {
  return (
    <div className="card">
      <div className="stat-label">{label}</div>
      <div className={hero ? 'hero-value' : 'stat-value'}>{value}</div>
      {delta != null && (
        <div className={`stat-delta ${delta >= 0 ? 'delta-up' : 'delta-down'}`}>
          {delta >= 0 ? '▲' : '▼'} {fmtCompact(Math.abs(delta))}
          {deltaLabel ? ` ${deltaLabel}` : ''}
        </div>
      )}
    </div>
  )
}

export function PnlText({ value, pct }: { value: number | null; pct?: number | null }) {
  if (value == null) return <>—</>
  const cls = value >= 0 ? 'delta-up' : 'delta-down'
  return (
    <span className={cls}>
      {fmtCompact(value)}
      {pct != null ? ` (${fmtPct(pct)})` : ''}
    </span>
  )
}

/** Excel download button — hits the backend export endpoint. */
export function ExportButton({ href, label = 'Export to Excel' }: { href: string; label?: string }) {
  return (
    <a className="btn" href={href} download>
      ⬇ {label}
    </a>
  )
}

/** Recharts custom tooltip that follows the design tokens. */
export function VizTooltip({
  active,
  payload,
  label,
  format,
  labelFormat,
}: {
  active?: boolean
  payload?: { name: string; value: number; color?: string; payload?: Record<string, unknown> }[]
  label?: string | number
  format: (v: number) => string
  labelFormat?: (l: string) => string
}) {
  if (!active || !payload?.length) return null
  return (
    <div className="viz-tooltip">
      <div className="tt-title">{labelFormat ? labelFormat(String(label)) : label}</div>
      {payload.map((p) => (
        <div className="tt-row" key={p.name}>
          <span className="tt-swatch" style={{ background: p.color }} />
          <span style={{ color: 'var(--text-secondary)' }}>{p.name}</span>
          <span style={{ marginLeft: 'auto', fontWeight: 600 }}>{format(p.value)}</span>
        </div>
      ))}
    </div>
  )
}

export function LegendRow({ items }: { items: { label: string; color: string }[] }) {
  return (
    <div className="legend-row">
      {items.map((it) => (
        <span className="legend-item" key={it.label}>
          <span className="tt-swatch" style={{ background: it.color }} />
          {it.label}
        </span>
      ))}
    </div>
  )
}
