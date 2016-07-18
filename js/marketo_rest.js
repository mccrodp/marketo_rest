(function($) {
  Drupal.behaviors.marketo_rest = {
    attach: function(context, settings) {
      // Only load Marketo Once.
      $(document).once('marketo', function() {
        // Only track Marketo if the setting is enabled.
        if (typeof settings.marketo_rest !== 'undefined' && settings.marketo_rest.track) {
          jQuery.ajax({
            url: document.location.protocol + settings.marketo_rest.library,
            dataType: 'script',
            cache: true,
            success: function() {
              Munchkin.init(settings.marketo_rest.key);
              if (typeof settings.marketo_rest.actions !== 'undefined') {
                jQuery.each(settings.marketo_rest.actions, function() {
                  Drupal.behaviors.marketo_rest.marketoMunchkinFunction(this.action, this.data, this.hash);
                });
              }
            }
          });
        }
      });
    },
    marketoMunchkinFunction: function(leadType, data, hash) {
      mktoMunchkinFunction(leadType, data, hash);
    }
  };

})(jQuery);
