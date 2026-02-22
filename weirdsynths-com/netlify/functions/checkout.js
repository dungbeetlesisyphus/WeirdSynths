// Netlify Serverless Function — Stripe Checkout
//
// Environment variables (set in Netlify dashboard):
//   STRIPE_SECRET_KEY — Stripe secret key
//
const stripe = require('stripe')(process.env.STRIPE_SECRET_KEY);
const BASE_URL = process.env.URL || 'https://facefront.netlify.app';

// ── BASE PRICES (USD cents) ─────────────────────────────────
const PLUGIN_PRICE_USD  = 5000;   // $50
const LIFETIME_PRICE_USD = 25600; // $256

// ── REGIONAL PRICING (purchasing power parity) ──────────────
// Multipliers relative to USD — lower = cheaper for that region
const REGIONAL_PRICING = {
  // Latin America
  BR: 0.40, MX: 0.50, AR: 0.30, CL: 0.50, CO: 0.40, PE: 0.40,
  // South/Southeast Asia
  IN: 0.30, PH: 0.35, ID: 0.30, VN: 0.30, TH: 0.40, MY: 0.45,
  // Eastern Europe
  PL: 0.55, RO: 0.45, HU: 0.50, CZ: 0.55, BG: 0.40, UA: 0.30,
  // Turkey / Middle East
  TR: 0.35, EG: 0.30, SA: 0.70, AE: 0.80,
  // Africa
  ZA: 0.40, NG: 0.30, KE: 0.30, GH: 0.30,
  // East Asia (lower cost)
  CN: 0.50, KR: 0.65, TW: 0.65,
  // Western Europe (near parity)
  GB: 0.90, DE: 0.90, FR: 0.90, NL: 0.90, SE: 0.90, NO: 0.90,
  ES: 0.85, IT: 0.85, PT: 0.75, GR: 0.70,
  // Oceania
  AU: 0.85, NZ: 0.80,
  // North America
  US: 1.00, CA: 0.85,
  // Japan
  JP: 0.75,
  // Russia / CIS
  RU: 0.35, KZ: 0.35,
};

function getRegionalPrice(basePriceUSD, countryCode) {
  const multiplier = REGIONAL_PRICING[countryCode] || 1.0;
  // Round to nearest 100 cents ($1 increments) for clean display
  return Math.max(100, Math.round((basePriceUSD * multiplier) / 100) * 100);
}

// ── COUNTRY DETECTION ───────────────────────────────────────
function getCountry(event) {
  // Netlify provides geo headers
  const country = event.headers['x-country'] ||
                  event.headers['x-nf-country-code'] ||
                  event.headers['cf-ipcountry'] || // Cloudflare fallback
                  'US';
  return country.toUpperCase();
}

exports.handler = async (event) => {
  if (event.httpMethod !== 'POST') {
    return { statusCode: 405, body: JSON.stringify({ error: 'Method not allowed' }) };
  }

  try {
    const body = JSON.parse(event.body);
    const country = getCountry(event);

    // ═══════════════════════════════════════
    //  PLUGIN SUITE — $50 base (regional)
    // ═══════════════════════════════════════
    if (body.plan === 'plugin') {
      const price = getRegionalPrice(PLUGIN_PRICE_USD, country);

      const session = await stripe.checkout.sessions.create({
        payment_method_types: ['card'],
        line_items: [{
          price_data: {
            currency: 'usd',
            product_data: {
              name: 'WeirdSynths Plugin Suite',
              description: 'All 8 current modules. Lifetime updates. Price rises with each new release.',
            },
            unit_amount: price,
            tax_behavior: 'inclusive', // We absorb sales tax
          },
          quantity: 1,
        }],
        mode: 'payment',
        automatic_tax: { enabled: true },
        success_url: `${BASE_URL}/success.html?session_id={CHECKOUT_SESSION_ID}&plan=plugin`,
        cancel_url: `${BASE_URL}/#pricing`,
        metadata: {
          plan: 'plugin-suite',
          items: 'face-cv,iris,emotionlfo,penrose,void,geofilter,portal,ossuary',
          country: country,
          base_price: PLUGIN_PRICE_USD.toString(),
          regional_price: price.toString(),
        },
      });

      return {
        statusCode: 200,
        body: JSON.stringify({ url: session.url }),
      };
    }

    // ═══════════════════════════════════════
    //  LIFETIME ACCESS — $256 base (regional)
    // ═══════════════════════════════════════
    if (body.plan === 'lifetime') {
      const price = getRegionalPrice(LIFETIME_PRICE_USD, country);

      const session = await stripe.checkout.sessions.create({
        payment_method_types: ['card'],
        line_items: [{
          price_data: {
            currency: 'usd',
            product_data: {
              name: 'WeirdSynths Lifetime Access',
              description: 'All current + all future modules. Every update, forever. Founding supporter.',
            },
            unit_amount: price,
            tax_behavior: 'inclusive', // We absorb sales tax
          },
          quantity: 1,
        }],
        mode: 'payment',
        automatic_tax: { enabled: true },
        success_url: `${BASE_URL}/success.html?session_id={CHECKOUT_SESSION_ID}&plan=lifetime`,
        cancel_url: `${BASE_URL}/#pricing`,
        metadata: {
          plan: 'lifetime-access',
          items: 'face-cv,iris,emotionlfo,penrose,void,geofilter,portal,ossuary',
          tier: 'founding',
          country: country,
          base_price: LIFETIME_PRICE_USD.toString(),
          regional_price: price.toString(),
        },
      });

      return {
        statusCode: 200,
        body: JSON.stringify({ url: session.url }),
      };
    }

    // ═══════════════════════════════════════
    //  GET REGIONAL PRICES (for frontend)
    // ═══════════════════════════════════════
    if (body.action === 'get-prices') {
      const pluginPrice = getRegionalPrice(PLUGIN_PRICE_USD, country);
      const lifetimePrice = getRegionalPrice(LIFETIME_PRICE_USD, country);

      return {
        statusCode: 200,
        body: JSON.stringify({
          country,
          plugin: pluginPrice,
          lifetime: lifetimePrice,
          isRegional: country !== 'US',
        }),
      };
    }

    // ═══════════════════════════════════════
    //  FREE (OSSUARY)
    // ═══════════════════════════════════════
    if (body.items && body.items.every(i => i.id === 'ossuary')) {
      return {
        statusCode: 200,
        body: JSON.stringify({
          url: `${BASE_URL}/success.html?free=ossuary`,
          free: true,
        }),
      };
    }

    return {
      statusCode: 400,
      body: JSON.stringify({ error: 'Please choose a plan from the pricing section' }),
    };

  } catch (error) {
    console.error('Checkout error:', error);
    return {
      statusCode: 500,
      body: JSON.stringify({ error: error.message }),
    };
  }
};
