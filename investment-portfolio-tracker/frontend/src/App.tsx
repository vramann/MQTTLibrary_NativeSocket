import { BrowserRouter, Route, Routes } from 'react-router-dom'
import { Layout } from './components/ui'
import AssetClassPage from './pages/AssetClassPage'
import Dashboard from './pages/Dashboard'
import Dividends from './pages/Dividends'
import Transactions from './pages/Transactions'

export default function App() {
  return (
    <BrowserRouter>
      <Layout>
        <Routes>
          <Route path="/" element={<Dashboard />} />
          <Route
            path="/equity"
            element={<AssetClassPage key="EQUITY" assetClass="EQUITY" title="Equity" sub="Listed shares — direct stock holdings" />}
          />
          <Route
            path="/mutual-funds"
            element={
              <AssetClassPage
                key="MUTUAL_FUND"
                assetClass="MUTUAL_FUND"
                title="Mutual Funds"
                sub="Lumpsum investments and SIPs"
              />
            }
          />
          <Route
            path="/gov-bonds"
            element={
              <AssetClassPage
                key="GOV_BOND"
                assetClass="GOV_BOND"
                title="Government Bonds"
                sub="G-Secs and treasury holdings with coupon income"
              />
            }
          />
          <Route
            path="/gold-bonds"
            element={
              <AssetClassPage
                key="GOLD_BOND"
                assetClass="GOLD_BOND"
                title="Gold Bonds (SGB)"
                sub="Sovereign Gold Bonds — grams held, 2.5% p.a. interest"
              />
            }
          />
          <Route path="/dividends" element={<Dividends />} />
          <Route path="/transactions" element={<Transactions />} />
        </Routes>
      </Layout>
    </BrowserRouter>
  )
}
