import { useEffect, useMemo, useState } from 'react'
import {
  Area,
  AreaChart,
  Bar,
  BarChart,
  CartesianGrid,
  Cell,
  Line,
  Pie,
  PieChart,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
} from 'recharts'
import type { ClassSummary, Dividend, DividendSummary, HistoryPoint } from '../lib/api'
import { CLASS_META, INCOME_META, fmtCompact, fmtMonth } from '../lib/api'
import { LegendRow, VizTooltip } from './ui'

/**
 * SVG presentation attributes can't consume CSS var(), so resolve the design
 * tokens once per color-scheme and hand raw hex to Recharts.
 */
function useTokens() {
  const read = () => {
    const cs = getComputedStyle(document.documentElement)
    const t = (name: string) => cs.getPropertyValue(name).trim()
    return {
      surface: t('--surface-1'),
      grid: t('--grid'),
      baseline: t('--baseline'),
      muted: t('--text-muted'),
      accent: t('--accent'),
      deemph: t('--deemph'),
      classes: {
        EQUITY: t('--c-equity'),
        MUTUAL_FUND: t('--c-mf'),
        GOV_BOND: t('--c-govbond'),
        GOLD_BOND: t('--c-goldbond'),
        OTHER: t('--c-other'),
      } as Record<string, string>,
    }
  }
  const [tokens, setTokens] = useState(read)
  useEffect(() => {
    const mq = window.matchMedia('(prefers-color-scheme: dark)')
    const onChange = () => setTokens(read())
    mq.addEventListener('change', onChange)
    return () => mq.removeEventListener('change', onChange)
  }, [])
  return tokens
}

const AXIS_TICK = { fontSize: 11.5 }
/* No entry animation: refetches must hold the previous render, not replay a sweep. */
const NO_ANIM = { isAnimationActive: false as const }

function fmtDateShort(d: string) {
  return new Date(d + 'T00:00:00').toLocaleString('en', { month: 'short', year: '2-digit' })
}

/** Portfolio value vs amount invested — emphasis form: value in accent, invested in gray. */
export function ValueTrendChart({ data }: { data: HistoryPoint[] }) {
  const tk = useTokens()
  return (
    <>
      <ResponsiveContainer width="100%" height={260}>
        <AreaChart data={data} margin={{ top: 8, right: 12, bottom: 0, left: 8 }}>
          <CartesianGrid stroke={tk.grid} strokeWidth={1} vertical={false} />
          <XAxis
            dataKey="date"
            tickFormatter={fmtDateShort}
            tick={{ ...AXIS_TICK, fill: tk.muted }}
            stroke={tk.baseline}
            tickLine={false}
            minTickGap={40}
          />
          <YAxis
            tickFormatter={(v: number) => fmtCompact(v)}
            tick={{ ...AXIS_TICK, fill: tk.muted }}
            stroke="transparent"
            width={58}
          />
          <Tooltip
            content={
              <VizTooltip
                format={fmtCompact}
                labelFormat={(l) => new Date(l + 'T00:00:00').toLocaleDateString('en-IN')}
              />
            }
          />
          <Area
            {...NO_ANIM}
            type="monotone"
            dataKey="value"
            name="Portfolio value"
            stroke={tk.accent}
            strokeWidth={2}
            fill={tk.accent}
            fillOpacity={0.1}
            dot={false}
            activeDot={{ r: 4, strokeWidth: 2, stroke: tk.surface }}
          />
          <Line
            {...NO_ANIM}
            type="monotone"
            dataKey="invested"
            name="Amount invested"
            stroke={tk.deemph}
            strokeWidth={2}
            dot={false}
          />
        </AreaChart>
      </ResponsiveContainer>
      <LegendRow
        items={[
          { label: 'Portfolio value', color: tk.accent },
          { label: 'Amount invested', color: tk.deemph },
        ]}
      />
    </>
  )
}

/** Allocation by asset class — part-to-whole donut, ≤5 segments, direct labels. */
export function AllocationDonut({ classes }: { classes: ClassSummary[] }) {
  const tk = useTokens()
  const data = classes
    .filter((c) => c.current_value > 0)
    .map((c) => ({
      name: CLASS_META[c.asset_class].label,
      value: c.current_value,
      color: tk.classes[c.asset_class],
    }))
  const total = data.reduce((a, d) => a + d.value, 0)
  return (
    <>
      <ResponsiveContainer width="100%" height={240}>
        <PieChart>
          <Pie
            {...NO_ANIM}
            data={data}
            dataKey="value"
            nameKey="name"
            innerRadius="58%"
            outerRadius="85%"
            paddingAngle={1.5}
            stroke="none"
          >
            {data.map((d) => (
              <Cell key={d.name} fill={d.color} />
            ))}
          </Pie>
          <Tooltip
            content={
              <VizTooltip format={(v) => `${fmtCompact(v)} · ${((v / total) * 100).toFixed(1)}%`} />
            }
          />
        </PieChart>
      </ResponsiveContainer>
      <LegendRow items={data.map((d) => ({ label: d.name, color: d.color }))} />
    </>
  )
}

/** Unrealized P&L per asset class — bars from a zero baseline, colored by entity. */
export function ClassPnlBars({ classes }: { classes: ClassSummary[] }) {
  const tk = useTokens()
  const data = classes.map((c) => ({
    name: CLASS_META[c.asset_class].label,
    pnl: c.unrealized_pnl,
    color: tk.classes[c.asset_class],
  }))
  return (
    <>
      <ResponsiveContainer width="100%" height={240}>
        <BarChart data={data} margin={{ top: 8, right: 12, bottom: 0, left: 8 }}>
          <CartesianGrid stroke={tk.grid} strokeWidth={1} vertical={false} />
          <XAxis
            dataKey="name"
            tick={{ ...AXIS_TICK, fill: tk.muted }}
            stroke={tk.baseline}
            tickLine={false}
            interval={0}
          />
          <YAxis
            tickFormatter={(v: number) => fmtCompact(v)}
            tick={{ ...AXIS_TICK, fill: tk.muted }}
            stroke="transparent"
            width={58}
          />
          <Tooltip cursor={{ fill: 'transparent' }} content={<VizTooltip format={fmtCompact} />} />
          <Bar {...NO_ANIM} dataKey="pnl" name="Unrealized P&L" barSize={22} radius={[4, 4, 0, 0]}>
            {data.map((d) => (
              <Cell key={d.name} fill={d.color} />
            ))}
          </Bar>
        </BarChart>
      </ResponsiveContainer>
    </>
  )
}

/** Monthly income stacked by type; income types wear their asset class hue. */
export function MonthlyIncomeChart({ summary }: { summary: DividendSummary }) {
  const tk = useTokens()
  const types: Dividend['type'][] = ['DIVIDEND', 'MF_PAYOUT', 'COUPON', 'SGB_INTEREST', 'OTHER']
  const typeColor: Record<string, string> = {
    DIVIDEND: tk.classes.EQUITY,
    MF_PAYOUT: tk.classes.MUTUAL_FUND,
    COUPON: tk.classes.GOV_BOND,
    SGB_INTEREST: tk.classes.GOLD_BOND,
    OTHER: tk.classes.OTHER,
  }
  const data = useMemo(() => {
    const byMonth = new Map<string, Record<string, number | string>>()
    for (const row of summary.by_month) {
      const cur = byMonth.get(row.month) ?? { month: row.month }
      cur[row.type] = ((cur[row.type] as number) ?? 0) + row.amount
      byMonth.set(row.month, cur)
    }
    return [...byMonth.values()].sort((a, b) => String(a.month).localeCompare(String(b.month)))
  }, [summary])

  const present = types.filter((t) => summary.by_month.some((r) => r.type === t))
  return (
    <>
      <ResponsiveContainer width="100%" height={260}>
        <BarChart data={data} margin={{ top: 8, right: 12, bottom: 0, left: 8 }}>
          <CartesianGrid stroke={tk.grid} strokeWidth={1} vertical={false} />
          <XAxis
            dataKey="month"
            tickFormatter={fmtMonth}
            tick={{ ...AXIS_TICK, fill: tk.muted }}
            stroke={tk.baseline}
            tickLine={false}
            minTickGap={28}
          />
          <YAxis
            tickFormatter={(v: number) => fmtCompact(v)}
            tick={{ ...AXIS_TICK, fill: tk.muted }}
            stroke="transparent"
            width={58}
          />
          <Tooltip
            cursor={{ fill: 'transparent' }}
            content={<VizTooltip format={fmtCompact} labelFormat={fmtMonth} />}
          />
          {present.map((t, i) => (
            <Bar
              {...NO_ANIM}
              key={t}
              dataKey={t}
              stackId="income"
              name={INCOME_META[t].label}
              fill={typeColor[t]}
              barSize={16}
              stroke={tk.surface}
              strokeWidth={1}
              radius={i === present.length - 1 ? [4, 4, 0, 0] : undefined}
            />
          ))}
        </BarChart>
      </ResponsiveContainer>
      <LegendRow items={present.map((t) => ({ label: INCOME_META[t].label, color: typeColor[t] }))} />
    </>
  )
}

/** Single-instrument price/NAV history sparkline area. */
export function PriceHistoryChart({
  data,
  assetClass,
}: {
  data: { date: string; price: number }[]
  assetClass?: string
}) {
  const tk = useTokens()
  const stroke = (assetClass && tk.classes[assetClass]) || tk.accent
  return (
    <ResponsiveContainer width="100%" height={220}>
      <AreaChart data={data} margin={{ top: 8, right: 12, bottom: 0, left: 8 }}>
        <CartesianGrid stroke={tk.grid} strokeWidth={1} vertical={false} />
        <XAxis
          dataKey="date"
          tickFormatter={fmtDateShort}
          tick={{ ...AXIS_TICK, fill: tk.muted }}
          stroke={tk.baseline}
          tickLine={false}
          minTickGap={40}
        />
        <YAxis
          domain={['auto', 'auto']}
          tickFormatter={(v: number) => fmtCompact(v)}
          tick={{ ...AXIS_TICK, fill: tk.muted }}
          stroke="transparent"
          width={58}
        />
        <Tooltip
          content={
            <VizTooltip
              format={(v) => `₹${v.toLocaleString('en-IN')}`}
              labelFormat={(l) => new Date(l + 'T00:00:00').toLocaleDateString('en-IN')}
            />
          }
        />
        <Area
          {...NO_ANIM}
          type="monotone"
          dataKey="price"
          name="Price"
          stroke={stroke}
          strokeWidth={2}
          fill={stroke}
          fillOpacity={0.1}
          dot={false}
          activeDot={{ r: 4, strokeWidth: 2, stroke: tk.surface }}
        />
      </AreaChart>
    </ResponsiveContainer>
  )
}
