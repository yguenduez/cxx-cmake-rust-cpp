//! Minimal client for the [Finnhub](https://finnhub.io) stock quote API.
//!
//! The API token is read from the `FINNHUB_API_KEY` environment variable.

use std::env;
use std::fmt;
use std::time::Duration;

use serde::Deserialize;

const BASE_URL: &str = "https://finnhub.io/api/v1";
const API_KEY_ENV: &str = "FINNHUB_API_KEY";
const TIMEOUT: Duration = Duration::from_secs(10);

/// A real-time quote returned by the Finnhub `/quote` endpoint.
#[derive(Debug, Clone, PartialEq, Deserialize)]
pub struct Quote {
    /// Current price.
    pub c: f64,
    /// Change since previous close.
    pub d: Option<f64>,
    /// Percent change since previous close.
    pub dp: Option<f64>,
    /// High price of the day.
    pub h: f64,
    /// Low price of the day.
    pub l: f64,
    /// Open price of the day.
    pub o: f64,
    /// Previous close price.
    pub pc: f64,
    /// Unix timestamp of the quote.
    pub t: i64,
}

/// Errors that can occur while fetching a quote.
#[derive(Debug)]
pub enum Error {
    /// `FINNHUB_API_KEY` was not set.
    MissingApiKey,
    /// The HTTP request failed (connection, TLS, timeout, ...).
    Http(ureq::Error),
    /// Finnhub returned an error response.
    Api(String),
    /// The symbol is unknown to Finnhub.
    SymbolNotFound(String),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::MissingApiKey => write!(f, "environment variable {API_KEY_ENV} is not set"),
            Error::Http(e) => write!(f, "http request failed: {e}"),
            Error::Api(msg) => write!(f, "finnhub api error: {msg}"),
            Error::SymbolNotFound(symbol) => write!(f, "symbol not found: {symbol}"),
        }
    }
}

impl std::error::Error for Error {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Error::Http(e) => Some(e),
            _ => None,
        }
    }
}

impl From<ureq::Error> for Error {
    fn from(e: ureq::Error) -> Self {
        Error::Http(e)
    }
}

#[derive(Deserialize)]
struct ApiError {
    error: String,
}

/// Fetch the current quote for `symbol`, e.g. `"AAPL"`.
pub fn quote(symbol: &str) -> Result<Quote, Error> {
    let token = env::var(API_KEY_ENV).map_err(|_| Error::MissingApiKey)?;

    let response = ureq::get(format!("{BASE_URL}/quote"))
        .query("symbol", symbol)
        .query("token", &token)
        .config()
        .timeout_global(Some(TIMEOUT))
        .http_status_as_error(false)
        .build()
        .call()?;

    let status = response.status();
    if !status.is_success() {
        let message = response
            .into_body()
            .read_json::<ApiError>()
            .map(|e| e.error)
            .unwrap_or_else(|_| status.to_string());
        return Err(Error::Api(message));
    }

    let quote: Quote = response.into_body().read_json()?;

    // Finnhub answers with an all-zero quote for unknown symbols.
    if quote.c == 0.0 && quote.t == 0 {
        return Err(Error::SymbolNotFound(symbol.to_string()));
    }

    Ok(quote)
}

/// Convenience wrapper returning only the current price of `symbol`.
pub fn current_price(symbol: &str) -> Result<f64, Error> {
    quote(symbol).map(|q| q.c)
}

/// FFI surface exposed to C++ through cxx.rs.
#[cxx::bridge(namespace = "rust_lib")]
mod ffi {
    /// Quote crossing the FFI boundary. cxx does not support `Option` yet, so
    /// `has_change` signals whether `change`/`percent_change` are meaningful.
    struct Quote {
        current: f64,
        change: f64,
        has_change: bool,
        percent_change: f64,
        high: f64,
        low: f64,
        open: f64,
        previous_close: f64,
        timestamp: i64,
    }

    extern "Rust" {
        fn current_price(symbol: &str) -> Result<f64>;
        fn stock_quote(symbol: &str) -> Result<Quote>;
    }
}

fn to_ffi_quote(quote: Quote) -> ffi::Quote {
    ffi::Quote {
        current: quote.c,
        change: quote.d.unwrap_or(0.0),
        has_change: quote.d.is_some(),
        percent_change: quote.dp.unwrap_or(0.0),
        high: quote.h,
        low: quote.l,
        open: quote.o,
        previous_close: quote.pc,
        timestamp: quote.t,
    }
}

/// Like [`quote`], but returns the FFI-friendly [`ffi::Quote`].
fn stock_quote(symbol: &str) -> Result<ffi::Quote, Error> {
    quote(symbol).map(to_ffi_quote)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn deserializes_quote() {
        let json = r#"{"c":189.84,"d":2.34,"dp":1.2481,"h":190.0,"l":186.5,"o":187.2,"pc":187.5,"t":1700000000}"#;
        let quote: Quote = serde_json::from_str(json).unwrap();
        assert_eq!(quote.c, 189.84);
        assert_eq!(quote.d, Some(2.34));
        assert_eq!(quote.dp, Some(1.2481));
        assert_eq!(quote.h, 190.0);
        assert_eq!(quote.l, 186.5);
        assert_eq!(quote.o, 187.2);
        assert_eq!(quote.pc, 187.5);
        assert_eq!(quote.t, 1_700_000_000);
    }

    #[test]
    fn deserializes_null_change() {
        let json = r#"{"c":0,"d":null,"dp":null,"h":0,"l":0,"o":0,"pc":0,"t":0}"#;
        let quote: Quote = serde_json::from_str(json).unwrap();
        assert_eq!(quote.c, 0.0);
        assert_eq!(quote.d, None);
        assert_eq!(quote.dp, None);
    }

    #[test]
    fn maps_quote_to_ffi() {
        let quote = Quote {
            c: 189.84,
            d: Some(2.34),
            dp: Some(1.2481),
            h: 190.0,
            l: 186.5,
            o: 187.2,
            pc: 187.5,
            t: 1_700_000_000,
        };
        let ffi = to_ffi_quote(quote);
        assert_eq!(ffi.current, 189.84);
        assert_eq!(ffi.change, 2.34);
        assert!(ffi.has_change);
        assert_eq!(ffi.percent_change, 1.2481);
        assert_eq!(ffi.timestamp, 1_700_000_000);
    }

    #[test]
    fn maps_null_change_to_ffi() {
        let quote = Quote {
            c: 10.0,
            d: None,
            dp: None,
            h: 11.0,
            l: 9.0,
            o: 10.0,
            pc: 10.0,
            t: 1_700_000_000,
        };
        let ffi = to_ffi_quote(quote);
        assert_eq!(ffi.change, 0.0);
        assert!(!ffi.has_change);
    }

    #[test]
    fn missing_api_key_errors() {
        // SAFETY: single-threaded test and we restore nothing since the test only
        // asserts on a missing variable. Guard against it being set in the env.
        if env::var(API_KEY_ENV).is_err() {
            assert!(matches!(current_price("AAPL"), Err(Error::MissingApiKey)));
        }
    }

    #[test]
    #[ignore = "requires network and a real FINNHUB_API_KEY"]
    fn live_quote() {
        let price = current_price("AAPL").unwrap();
        assert!(price > 0.0);
    }
}
