// Netlify Serverless Function — Verify Stripe Session
//
// Called by success.html to determine what was purchased
// Returns only the module IDs the customer is entitled to download
//
const stripe = require('stripe')(process.env.STRIPE_SECRET_KEY);

const ALL_MODULES = ['face-cv', 'iris', 'emotionlfo', 'penrose', 'void', 'geofilter', 'portal', 'ossuary'];

exports.handler = async (event) => {
  if (event.httpMethod !== 'POST') {
    return { statusCode: 405, body: JSON.stringify({ error: 'Method not allowed' }) };
  }

  try {
    const { session_id } = JSON.parse(event.body);

    if (!session_id) {
      return { statusCode: 400, body: JSON.stringify({ error: 'No session ID' }) };
    }

    // Retrieve the Checkout Session from Stripe
    const session = await stripe.checkout.sessions.retrieve(session_id);

    if (session.payment_status !== 'paid') {
      return { statusCode: 402, body: JSON.stringify({ error: 'Payment not completed' }) };
    }

    let modules = [];
    let plan = null;

    // Check if this is a subscription (all-access) or bundle
    const sessionPlan = session.metadata?.plan;

    if (sessionPlan === 'plugin-suite' || sessionPlan === 'lifetime-access') {
      // Full access — all modules
      modules = ALL_MODULES;
      plan = sessionPlan;
    } else {
      // Individual purchase — only purchased modules
      const purchasedItems = session.metadata?.items;
      const freeItems = session.metadata?.free_items;

      if (purchasedItems) modules = purchasedItems.split(',');
      if (freeItems) modules = [...modules, ...freeItems.split(',')];
    }

    // Filter to valid modules only
    modules = modules.filter(id => ALL_MODULES.includes(id));

    return {
      statusCode: 200,
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        modules,
        plan,
        customer_email: session.customer_details?.email || null,
      }),
    };

  } catch (error) {
    console.error('Verify session error:', error);
    return {
      statusCode: 500,
      body: JSON.stringify({ error: 'Could not verify session' }),
    };
  }
};
