// Netlify Serverless Function — Stripe Checkout Session
//
// Environment variables required (set in Netlify dashboard):
//   STRIPE_SECRET_KEY — your Stripe secret key
//   STRIPE_TAX_ENABLED — set to 'true' to enable Stripe Tax (we absorb it)
//
const stripe = require('stripe')(process.env.STRIPE_SECRET_KEY);

const BASE_URL = process.env.URL || 'https://facefront.netlify.app';

// Pricing
const PLUGIN_PRICE = 5000;   // $50 — full plugin suite (all current modules)
const LIFETIME_PRICE = 25600; // $256 — lifetime access (all current + all future)

// Tax config
const taxConfig = process.env.STRIPE_TAX_ENABLED === 'true'
  ? { automatic_tax: { enabled: true } }
  : {};

exports.handler = async (event) => {
  if (event.httpMethod !== 'POST') {
    return { statusCode: 405, body: JSON.stringify({ error: 'Method not allowed' }) };
  }

  try {
    const body = JSON.parse(event.body);

    // ═══════════════════════════════════════
    //  PLUGIN SUITE — $50
    // ═══════════════════════════════════════
    if (body.plan === 'plugin') {
      const session = await stripe.checkout.sessions.create({
        payment_method_types: ['card'],
        line_items: [{
          price_data: {
            currency: 'usd',
            product_data: {
              name: 'WeirdSynths Plugin Suite',
              description: 'All 8 current modules. Lifetime updates on current modules. Price rises with each new release.',
            },
            unit_amount: PLUGIN_PRICE,
            tax_behavior: 'inclusive',
          },
          quantity: 1,
        }],
        mode: 'payment',
        ...taxConfig,
        success_url: `${BASE_URL}/success.html?session_id={CHECKOUT_SESSION_ID}&plan=plugin`,
        cancel_url: `${BASE_URL}/#pricing`,
        metadata: {
          plan: 'plugin-suite',
          items: 'face-cv,iris,emotionlfo,penrose,void,geofilter,portal,ossuary',
        },
      });

      return {
        statusCode: 200,
        body: JSON.stringify({ url: session.url }),
      };
    }

    // ═══════════════════════════════════════
    //  LIFETIME ACCESS — $256
    // ═══════════════════════════════════════
    if (body.plan === 'lifetime') {
      const session = await stripe.checkout.sessions.create({
        payment_method_types: ['card'],
        line_items: [{
          price_data: {
            currency: 'usd',
            product_data: {
              name: 'WeirdSynths Lifetime Access',
              description: 'All current + all future modules. Every update, forever. Founding supporter.',
            },
            unit_amount: LIFETIME_PRICE,
            tax_behavior: 'inclusive',
          },
          quantity: 1,
        }],
        mode: 'payment',
        ...taxConfig,
        success_url: `${BASE_URL}/success.html?session_id={CHECKOUT_SESSION_ID}&plan=lifetime`,
        cancel_url: `${BASE_URL}/#pricing`,
        metadata: {
          plan: 'lifetime-access',
          items: 'face-cv,iris,emotionlfo,penrose,void,geofilter,portal,ossuary',
          tier: 'founding',
        },
      });

      return {
        statusCode: 200,
        body: JSON.stringify({ url: session.url }),
      };
    }

    // ═══════════════════════════════════════
    //  INDIVIDUAL MODULE CART (fallback)
    // ═══════════════════════════════════════
    const { items } = body;

    if (!items || !items.length) {
      return { statusCode: 400, body: JSON.stringify({ error: 'No items provided' }) };
    }

    // Free items (OSSUARY) — direct download
    const freeOnly = items.every(item => item.id === 'ossuary');
    if (freeOnly) {
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
