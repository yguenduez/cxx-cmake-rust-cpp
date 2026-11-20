//! Minimal client for the [Finnhub](https://finnhub.io) stock quote API.
//!
//! The API token is read from the `FINNHUB_API_KEY` environment variable.

use std::env;
use std::fmt;
use std::time::Duration;

use serde::Deserialize;

const BASE_URL: &str = "https://finnhub.io/api/v1";
const API_KEY_ENV: &str = "FINNHUB_API_KEY";
const TOKEN_HEADER: &str = "X-Finnhub-Token";
const TIMEOUT: Duration = Duration::from_secs(10);

/// A real-time quote returned by the Finnhub `/quote` endpoint.
#[derive(Debug, Clone, PartialEq, Deserialize)]
pub struct Quote {
    /// Current price.
    #[serde(rename = "c")]
    pub current: f64,
    /// Change since previous close.
    #[serde(rename = "d")]
    pub change: Option<f64>,
    /// Percent change since previous close.
    #[serde(rename = "dp")]
    pub percent_change: Option<f64>,
    /// High price of the day.
    #[serde(rename = "h")]
    pub high: f64,
    /// Low price of the day.
    #[serde(rename = "l")]
    pub low: f64,
    /// Open price of the day.
    #[serde(rename = "o")]
    pub open: f64,
    /// Previous close price.
    #[serde(rename = "pc")]
    pub previous_close: f64,
    /// Unix timestamp of the quote.
    #[serde(rename = "t")]
    pub timestamp: i64,
}

/// Errors that can occur while fetching a quote.
#[derive(Debug)]
pub enum Error {
    /// `FINNHUB_API_KEY` was not set.
    MissingApiKey,
    /// `FINNHUB_API_KEY` is set but not valid Unicode.
    InvalidApiKey,
    /// The HTTP request failed (connection, TLS, timeout, ...).
    Http(ureq::Error),
    /// Finnhub answered successfully, but the body is not a valid quote.
    InvalidResponse(ureq::Error),
    /// Finnhub returned an error response.
    Api(String),
    /// The symbol is unknown to Finnhub.
    SymbolNotFound(String),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::MissingApiKey => write!(f, "environment variable {API_KEY_ENV} is not set"),
            Error::InvalidApiKey => {
                write!(f, "environment variable {API_KEY_ENV} is not valid unicode")
            }
            Error::Http(e) => write!(f, "http request failed: {e}"),
            Error::InvalidResponse(e) => write!(f, "invalid response from finnhub: {e}"),
            Error::Api(msg) => write!(f, "finnhub api error: {msg}"),
            Error::SymbolNotFound(symbol) => write!(f, "symbol not found: {symbol}"),
        }
    }
}

// No `source()`: `Display` already includes the cause, because cxx builds
// `rust::Error::what()` from `Display` alone.
impl std::error::Error for Error {}

#[derive(Deserialize)]
struct ApiError {
    error: String,
}

/// Fetch the current quote for `symbol`, e.g. `"AAPL"`.
///
/// Reads the API key from `FINNHUB_API_KEY` and blocks for up to 10 seconds.
///
/// # Errors
///
/// - [`Error::MissingApiKey`] if `FINNHUB_API_KEY` is not set.
/// - [`Error::InvalidApiKey`] if `FINNHUB_API_KEY` is not valid Unicode.
/// - [`Error::Http`] if the request fails or times out.
/// - [`Error::Api`] if Finnhub returns a non-success status.
/// - [`Error::InvalidResponse`] if the body is not a valid quote.
/// - [`Error::SymbolNotFound`] if Finnhub does not know `symbol`.
pub fn quote(symbol: &str) -> Result<Quote, Error> {
    let token = api_key(|name| env::var(name))?;

    let response = ureq::get(format!("{BASE_URL}/quote"))
        .query("symbol", symbol)
        .header(TOKEN_HEADER, &token)
        .config()
        .timeout_global(Some(TIMEOUT))
        .http_status_as_error(false)
        .build()
        .call()
        .map_err(Error::Http)?;

    let status = response.status();
    if !status.is_success() {
        let message = response
            .into_body()
            .read_json::<ApiError>()
            .map_or_else(|_| status.to_string(), |e| e.error);
        return Err(Error::Api(message));
    }

    let quote: Quote = response
        .into_body()
        .read_json()
        .map_err(Error::InvalidResponse)?;

    reject_unknown_symbol(symbol, quote)
}

/// Read the API key through `lookup`, so tests don't depend on the environment.
fn api_key(lookup: impl FnOnce(&str) -> Result<String, env::VarError>) -> Result<String, Error> {
    lookup(API_KEY_ENV).map_err(|e| match e {
        env::VarError::NotPresent => Error::MissingApiKey,
        env::VarError::NotUnicode(_) => Error::InvalidApiKey,
    })
}

/// Finnhub answers with an all-zero quote for unknown symbols.
fn reject_unknown_symbol(symbol: &str, quote: Quote) -> Result<Quote, Error> {
    if quote.current == 0.0 && quote.timestamp == 0 {
        return Err(Error::SymbolNotFound(symbol.to_string()));
    }
    Ok(quote)
}

/// FFI surface exposed to C++ through cxx.rs.
#[cxx::bridge(namespace = "rust_lib")]
mod ffi {
    struct Quote {
        current: f64,
        /// `change` and `percent_change` are only meaningful if this is set.
        has_change: bool,
        change: f64,
        percent_change: f64,
        high: f64,
        low: f64,
        open: f64,
        previous_close: f64,
        timestamp: i64,
    }

    extern "Rust" {
        fn stock_quote(symbol: &str) -> Result<Quote>;
    }
}

impl From<Quote> for ffi::Quote {
    fn from(quote: Quote) -> Self {
        let change = quote.change.zip(quote.percent_change);
        let (change_value, percent_change) = change.unwrap_or_default();
        ffi::Quote {
            current: quote.current,
            has_change: change.is_some(),
            change: change_value,
            percent_change,
            high: quote.high,
            low: quote.low,
            open: quote.open,
            previous_close: quote.previous_close,
            timestamp: quote.timestamp,
        }
    }
}

/// Like [`quote`], but returns the FFI-friendly [`ffi::Quote`].
fn stock_quote(symbol: &str) -> Result<ffi::Quote, Error> {
    quote(symbol).map(ffi::Quote::from)
}

#[cfg(test)]
mod tests {
    use super::*;

    use std::ffi::OsString;

    fn sample_quote() -> Quote {
        Quote {
            current: 189.84,
            change: Some(2.34),
            percent_change: Some(1.2481),
            high: 190.0,
            low: 186.5,
            open: 187.2,
            previous_close: 187.5,
            timestamp: 1_700_000_000,
        }
    }

    fn unknown_symbol_quote() -> Quote {
        Quote {
            current: 0.0,
            change: None,
            percent_change: None,
            high: 0.0,
            low: 0.0,
            open: 0.0,
            previous_close: 0.0,
            timestamp: 0,
        }
    }

    #[test]
    fn deserializes_quote() {
        let json = r#"{"c":189.84,"d":2.34,"dp":1.2481,"h":190.0,"l":186.5,"o":187.2,"pc":187.5,"t":1700000000}"#;

        let quote: Quote = serde_json::from_str(json).unwrap();

        assert_eq!(quote, sample_quote());
    }

    #[test]
    fn deserializes_null_change() {
        let json = r#"{"c":0,"d":null,"dp":null,"h":0,"l":0,"o":0,"pc":0,"t":0}"#;

        let quote: Quote = serde_json::from_str(json).unwrap();

        assert_eq!(quote, unknown_symbol_quote());
    }

    #[test]
    fn maps_quote_to_ffi() {
        let ffi = ffi::Quote::from(sample_quote());

        assert_eq!(ffi.current, 189.84);
        assert!(ffi.has_change);
        assert_eq!(ffi.change, 2.34);
        assert_eq!(ffi.percent_change, 1.2481);
        assert_eq!(ffi.high, 190.0);
        assert_eq!(ffi.low, 186.5);
        assert_eq!(ffi.open, 187.2);
        assert_eq!(ffi.previous_close, 187.5);
        assert_eq!(ffi.timestamp, 1_700_000_000);
    }

    #[test]
    fn maps_null_change_to_ffi() {
        let quote = Quote {
            change: None,
            percent_change: None,
            ..sample_quote()
        };

        let ffi = ffi::Quote::from(quote);

        assert!(!ffi.has_change);
        assert_eq!(ffi.change, 0.0);
        assert_eq!(ffi.percent_change, 0.0);
    }

    #[test]
    fn maps_missing_percent_change_to_no_change() {
        let quote = Quote {
            percent_change: None,
            ..sample_quote()
        };

        let ffi = ffi::Quote::from(quote);

        assert!(!ffi.has_change);
    }

    #[test]
    fn missing_api_key_errors() {
        let result = api_key(|_| Err(env::VarError::NotPresent));

        assert!(matches!(result, Err(Error::MissingApiKey)));
    }

    #[test]
    fn non_unicode_api_key_errors() {
        let result = api_key(|_| Err(env::VarError::NotUnicode(OsString::new())));

        assert!(matches!(result, Err(Error::InvalidApiKey)));
    }

    #[test]
    fn reads_api_key_from_lookup() {
        let key = api_key(|name| match name {
            API_KEY_ENV => Ok("secret".to_string()),
            _ => Err(env::VarError::NotPresent),
        });

        assert_eq!(key.unwrap(), "secret");
    }

    #[test]
    fn rejects_all_zero_quote_as_unknown_symbol() {
        let result = reject_unknown_symbol("NOPE", unknown_symbol_quote());

        assert!(matches!(result, Err(Error::SymbolNotFound(symbol)) if symbol == "NOPE"));
    }

    #[test]
    fn accepts_known_symbol_quote() {
        let result = reject_unknown_symbol("AAPL", sample_quote());

        assert_eq!(result.unwrap(), sample_quote());
    }

    #[test]
    #[ignore = "requires network and a real FINNHUB_API_KEY"]
    fn live_quote() {
        let quote = quote("AAPL").unwrap();
        assert!(quote.current > 0.0);
    }
}
