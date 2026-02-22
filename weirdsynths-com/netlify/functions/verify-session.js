// Verify Stripe Session — returns purchased module IDs
const stripe = require('stripe')(process.env.STRIPE_SECRET_KEY);
const ALL_MODULES = ['face-cv','iris','emotionlfo','penrose','void','geofilter','portal','ossuary'];

exports.handler = async (event) => {
  if (event.httpMethod !== 'POST') {
    return { statusCode: 405, body: JSON.stringify({ error: 'Method not allowed' }) };
  }
  try {
    const { session_id } = JSON.parse(event.body);
    if (!session_id) return { statusCode: 400, body: JSON.stringify({ error: 'No session ID' }) };

    const session = await stripe.checkout.sessions.retrieve(session_id);
    if (session.payment_status !== 'paid') {
      return { statusCode: 402, body: JSON.stringify({ error: 'Payment not completed' }) };
    }

    const sessionPlan = session.metadata?.plan;
    let modules = [];
    let plan = null;

    if (sessionPlan === 'plugin-suite' || sessionPlan === 'lifetime-access') {
      modules = ALL_MODULES;
      plan = sessionPlan;
    } else {
      const items = session.metadata?.items;
      if (items) modules = items.split(',');
    }

    modules = modules.filter(id => ALL_MODULES.includes(id));

    return {
      statusCode: 200,
      body: JSON.stringify({ modules, plan, customer_email: session.customer_details?.email }),
    };
  } catch (error) {
    return { statusCode: 500, body: JSON.stringify({ error: 'Could not verify session' }) };
  }
};
